#include "AccountManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QCoreApplication>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {

constexpr int kPbkdf2Iterations = 200000;
constexpr int kKeyLength        = 32;
constexpr int kSaltLength       = 16;
constexpr int kIvLength         = 12;
constexpr int kTagLength        = 16;

const QString kDefaultSearchEngine = QStringLiteral("https://www.google.com/search?q=%s");
const QString kActiveProfileKey    = QStringLiteral("active_profile_id");

} // namespace

AccountManager::AccountManager(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_connectionName(QStringLiteral("warden_accounts_%1")
                           .arg(reinterpret_cast<quintptr>(this)))
{
    qRegisterMetaType<UserProfile>("UserProfile");
}

AccountManager::~AccountManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool AccountManager::initialize(const QString& dataLocation)
{
    m_dataLocation = dataLocation;

    if (!loadCloudConfig()) {
        qWarning() << "AccountManager: cloud_config.json not found or invalid, "
                      "cloud sync will be disabled";
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);

    const QString dbDir = m_dataLocation + QStringLiteral("/databases");
    if (!QDir().mkpath(dbDir)) {
        qCritical() << "AccountManager: cannot create database directory:" << dbDir;
        return false;
    }

    const QString dbPath = dbDir + QStringLiteral("/accounts.db");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "AccountManager: cannot open database:"
                    << m_db.lastError().text();
        return false;
    }

    if (!createTables()) {
        qCritical() << "AccountManager: cannot create tables";
        return false;
    }

    return true;
}

bool AccountManager::hasActiveSession() const
{
    if (!m_currentProfileId.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), kActiveProfileKey);

    if (!query.exec() || !query.next()) {
        return false;
    }

    const QString id = query.value(0).toString();

    QSqlQuery check(m_db);
    check.prepare(QStringLiteral("SELECT id FROM profiles WHERE id = :id"));
    check.bindValue(QStringLiteral(":id"), id);

    return check.exec() && check.next();
}

bool AccountManager::registerUser(const QString& username,
                                  const QString& email,
                                  const QString& password,
                                  bool enableCloudSync)
{
    if (username.isEmpty() || email.isEmpty() || password.isEmpty()) {
        emit authError(QStringLiteral("Username, email and password are required"));
        return false;
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QByteArray salt(kSaltLength, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), salt.size()) != 1) {
        emit authError(QStringLiteral("Failed to generate a secure salt"));
        return false;
    }

    const QByteArray hash = deriveKey(password.toUtf8(), salt);
    if (hash.isEmpty()) {
        emit authError(QStringLiteral("Failed to derive the password key"));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO profiles "
        "(id, username, email, search_engine, is_guest, cloud_sync_enabled, "
        " password_salt, password_hash) "
        "VALUES (:id, :username, :email, :search, 0, :cloud, :salt, :hash)"));

    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":email"), email);
    query.bindValue(QStringLiteral(":search"), kDefaultSearchEngine);
    query.bindValue(QStringLiteral(":cloud"), enableCloudSync ? 1 : 0);
    query.bindValue(QStringLiteral(":salt"), QString::fromLatin1(salt.toHex()));
    query.bindValue(QStringLiteral(":hash"), QString::fromLatin1(hash.toHex()));

    if (!query.exec()) {
        emit authError(QStringLiteral("Failed to create the profile: ")
                       + query.lastError().text());
        return false;
    }

    setActiveProfile(id);

    if (enableCloudSync && isCloudConfigured()) {
        signUpWithSupabase(email, password, username);
    }

    const UserProfile profile = loadProfile(id);
    emit authSuccess(profile);
    return true;
}

bool AccountManager::loginUser(const QString& email, const QString& password)
{
    if (email.isEmpty() || password.isEmpty()) {
        emit authError(QStringLiteral("Email and password are required"));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, password_salt, password_hash "
        "FROM profiles WHERE email = :email AND is_guest = 0"));
    query.bindValue(QStringLiteral(":email"), email);

    if (!query.exec() || !query.next()) {
        emit authError(QStringLiteral("Invalid email or password"));
        return false;
    }

    const QString id          = query.value(0).toString();
    const QByteArray salt     = QByteArray::fromHex(query.value(1).toString().toLatin1());
    const QByteArray expected = QByteArray::fromHex(query.value(2).toString().toLatin1());

    const QByteArray actual = deriveKey(password.toUtf8(), salt);
    if (actual.isEmpty() || actual != expected) {
        emit authError(QStringLiteral("Invalid email or password"));
        return false;
    }

    setActiveProfile(id);

    const UserProfile profile = loadProfile(id);
    emit authSuccess(profile);

    if (profile.cloudSyncEnabled && isCloudConfigured()) {
        loginWithSupabase(email, password);
    }

    return true;
}

bool AccountManager::loginAsGuest()
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT id FROM profiles WHERE is_guest = 1 LIMIT 1"));

    QString id;
    if (query.exec() && query.next()) {
        id = query.value(0).toString();
    } else {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral(
            "INSERT INTO profiles "
            "(id, username, email, search_engine, is_guest, cloud_sync_enabled) "
            "VALUES (:id, 'Guest', NULL, :search, 1, 0)"));
        insert.bindValue(QStringLiteral(":id"), id);
        insert.bindValue(QStringLiteral(":search"), kDefaultSearchEngine);

        if (!insert.exec()) {
            emit authError(QStringLiteral("Failed to create a guest session: ")
                           + insert.lastError().text());
            return false;
        }
    }

    setActiveProfile(id);

    const UserProfile profile = loadProfile(id);
    emit authSuccess(profile);
    return true;
}

void AccountManager::logout()
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), kActiveProfileKey);
    query.exec();

    m_currentProfileId.clear();
    m_accessToken.clear();
}

UserProfile AccountManager::currentUser() const
{
    if (m_currentProfileId.isEmpty()) {
        return {};
    }
    return loadProfile(m_currentProfileId);
}

bool AccountManager::setSearchEngine(const QString& engine)
{
    if (m_currentProfileId.isEmpty() || engine.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE profiles SET search_engine = :engine WHERE id = :id"));
    query.bindValue(QStringLiteral(":engine"), engine);
    query.bindValue(QStringLiteral(":id"), m_currentProfileId);

    return query.exec();
}

QByteArray AccountManager::deriveKey(const QByteArray& password,
                                     const QByteArray& salt) const
{
    unsigned char key[kKeyLength];

    if (PKCS5_PBKDF2_HMAC(password.constData(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.constData()),
                          static_cast<int>(salt.size()),
                          kPbkdf2Iterations,
                          EVP_sha256(),
                          kKeyLength,
                          key) != 1) {
        qWarning() << "AccountManager: PKCS5_PBKDF2_HMAC failed";
        return {};
    }

    return QByteArray(reinterpret_cast<const char*>(key), kKeyLength);
}

QByteArray AccountManager::encryptBlob(const QByteArray& plaintext,
                                       const QByteArray& password,
                                       QByteArray& salt,
                                       QByteArray& iv,
                                       QByteArray& tag) const
{
    salt.resize(kSaltLength);
    iv.resize(kIvLength);
    tag.resize(kTagLength);

    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), salt.size()) != 1 ||
        RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), iv.size()) != 1) {
        qWarning() << "AccountManager: RAND_bytes failed";
        return {};
    }

    const QByteArray key = deriveKey(password, salt);
    if (key.isEmpty()) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "AccountManager: EVP_CIPHER_CTX_new failed";
        return {};
    }

    QByteArray ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int len = 0;
    int totalLen = 0;
    bool ok = true;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        ok = false;
    }
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        ok = false;
    }
    if (ok && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                                 reinterpret_cast<const unsigned char*>(key.constData()),
                                 reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        ok = false;
    }
    if (ok && EVP_EncryptUpdate(ctx,
                                reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                                reinterpret_cast<const unsigned char*>(plaintext.constData()),
                                static_cast<int>(plaintext.size())) != 1) {
        ok = false;
    } else if (ok) {
        totalLen = len;
    }
    if (ok && EVP_EncryptFinal_ex(ctx,
                                  reinterpret_cast<unsigned char*>(ciphertext.data()) + totalLen,
                                  &len) != 1) {
        ok = false;
    } else if (ok) {
        totalLen += len;
    }
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
        ok = false;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        qWarning() << "AccountManager: encryption failed";
        return {};
    }

    ciphertext.resize(totalLen);
    return ciphertext;
}

QByteArray AccountManager::decryptBlob(const QByteArray& ciphertext,
                                       const QByteArray& password,
                                       const QByteArray& salt,
                                       const QByteArray& iv,
                                       const QByteArray& tag) const
{
    if (salt.isEmpty() || iv.isEmpty() || tag.isEmpty()) {
        qWarning() << "AccountManager: missing salt/iv/tag";
        return {};
    }

    const QByteArray key = deriveKey(password, salt);
    if (key.isEmpty()) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "AccountManager: EVP_CIPHER_CTX_new failed";
        return {};
    }

    QByteArray plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int len = 0;
    int totalLen = 0;
    bool ok = true;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        ok = false;
    }
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        ok = false;
    }
    if (ok && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                                 reinterpret_cast<const unsigned char*>(key.constData()),
                                 reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        ok = false;
    }
    if (ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                                  const_cast<char*>(tag.constData())) != 1) {
        ok = false;
    }
    if (ok && EVP_DecryptUpdate(ctx,
                                reinterpret_cast<unsigned char*>(plaintext.data()), &len,
                                reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                                static_cast<int>(ciphertext.size())) != 1) {
        ok = false;
    } else if (ok) {
        totalLen = len;
    }
    if (ok && EVP_DecryptFinal_ex(ctx,
                                  reinterpret_cast<unsigned char*>(plaintext.data()) + totalLen,
                                  &len) != 1) {
        ok = false;
    } else if (ok) {
        totalLen += len;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        qWarning() << "AccountManager: decryption failed (authentication or data error)";
        return {};
    }

    plaintext.resize(totalLen);
    return plaintext;
}

void AccountManager::syncToCloud()
{
    if (!isCloudConfigured()) {
        emit syncCompleted(false);
        return;
    }

    const UserProfile profile = currentUser();
    if (profile.id.isEmpty()) {
        emit syncCompleted(false);
        return;
    }

    pushProfileToCloud();
}

bool AccountManager::loadCloudConfig()
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/cloud_config.json"),
        QDir::currentPath() + QStringLiteral("/cloud_config.json"),
    };

    QString path;
    for (const QString& candidate : candidates) {
        if (QFile::exists(candidate)) {
            path = candidate;
            break;
        }
    }

    if (path.isEmpty()) {
        qWarning() << "AccountManager: cloud_config.json not found";
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AccountManager: cannot open" << path;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "AccountManager: invalid cloud_config.json";
        return false;
    }

    const QJsonObject obj = doc.object();
    m_supabaseUrl     = obj.value(QStringLiteral("supabase_url")).toString();
    m_supabaseAnonKey = obj.value(QStringLiteral("supabase_anon_key")).toString();

    const bool configured =
        !m_supabaseUrl.isEmpty() &&
        !m_supabaseAnonKey.isEmpty() &&
        m_supabaseUrl != QStringLiteral("YOUR_SUPABASE_URL") &&
        m_supabaseAnonKey != QStringLiteral("YOUR_SUPABASE_PUBLISHABLE_KEY");

    if (!configured) {
        qWarning() << "AccountManager: Supabase is not configured";
        return false;
    }

    return true;
}

bool AccountManager::createTables()
{
    const QString profilesSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS profiles ("
        "  id TEXT PRIMARY KEY,"
        "  username TEXT NOT NULL,"
        "  email TEXT,"
        "  search_engine TEXT NOT NULL DEFAULT 'https://www.google.com/search?q=%s',"
        "  is_guest INTEGER NOT NULL DEFAULT 0,"
        "  cloud_sync_enabled INTEGER NOT NULL DEFAULT 0,"
        "  password_salt TEXT,"
        "  password_hash TEXT,"
        "  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ")");

    const QString settingsSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT"
        ")");

    const QString bookmarksSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS bookmarks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  profile_id TEXT NOT NULL,"
        "  url TEXT NOT NULL,"
        "  title TEXT,"
        "  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ")");

    QSqlQuery query(m_db);

    if (!query.exec(profilesSql)) {
        qCritical() << "AccountManager: profiles table:" << query.lastError().text();
        return false;
    }
    if (!query.exec(settingsSql)) {
        qCritical() << "AccountManager: settings table:" << query.lastError().text();
        return false;
    }
    if (!query.exec(bookmarksSql)) {
        qCritical() << "AccountManager: bookmarks table:" << query.lastError().text();
        return false;
    }

    return true;
}

bool AccountManager::isCloudConfigured() const
{
    return !m_supabaseUrl.isEmpty() && !m_supabaseAnonKey.isEmpty();
}

UserProfile AccountManager::loadProfile(const QString& id) const
{
    UserProfile profile;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, username, email, search_engine, is_guest, cloud_sync_enabled "
        "FROM profiles WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec() || !query.next()) {
        return {};
    }

    profile.id               = query.value(0).toString();
    profile.username         = query.value(1).toString();
    profile.email            = query.value(2).toString();
    profile.searchEngine     = query.value(3).toString();
    profile.isGuest          = query.value(4).toBool();
    profile.cloudSyncEnabled = query.value(5).toBool();

    return profile;
}

void AccountManager::setActiveProfile(const QString& id)
{
    m_currentProfileId = id;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO settings (key, value) VALUES (:key, :id)"));
    query.bindValue(QStringLiteral(":key"), kActiveProfileKey);
    query.bindValue(QStringLiteral(":id"), id);
    query.exec();
}

void AccountManager::signUpWithSupabase(const QString& email,
                                        const QString& password,
                                        const QString& username)
{
    QNetworkRequest request(QUrl(m_supabaseUrl + QStringLiteral("/auth/v1/signup")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("apikey", m_supabaseAnonKey.toUtf8());

    QJsonObject body;
    body.insert(QStringLiteral("email"), email);
    body.insert(QStringLiteral("password"), password);
    body.insert(QStringLiteral("data"),
                QJsonObject{{QStringLiteral("username"), username}});

    QNetworkReply* reply =
        m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit authError(QStringLiteral("Cloud signup failed: ")
                           + reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject obj = doc.object();
        m_accessToken = obj.value(QStringLiteral("access_token")).toString();

        pushProfileToCloud();
    });
}

void AccountManager::loginWithSupabase(const QString& email, const QString& password)
{
    QUrl url(m_supabaseUrl + QStringLiteral("/auth/v1/token"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("password"));
    url.setQuery(urlQuery);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("apikey", m_supabaseAnonKey.toUtf8());

    QJsonObject body;
    body.insert(QStringLiteral("email"), email);
    body.insert(QStringLiteral("password"), password);

    QNetworkReply* reply =
        m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit authError(QStringLiteral("Cloud login failed: ")
                           + reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject obj = doc.object();
        m_accessToken = obj.value(QStringLiteral("access_token")).toString();

        emit syncCompleted(true);
    });
}

void AccountManager::pushProfileToCloud()
{
    const UserProfile profile = currentUser();
    if (profile.id.isEmpty()) {
        emit syncCompleted(false);
        return;
    }

    QJsonObject blob;
    blob.insert(QStringLiteral("profile_id"), profile.id);
    blob.insert(QStringLiteral("username"), profile.username);
    blob.insert(QStringLiteral("email"), profile.email);
    blob.insert(QStringLiteral("search_engine"), profile.searchEngine);
    blob.insert(QStringLiteral("cloud_sync_enabled"), profile.cloudSyncEnabled);

    const QByteArray plaintext =
        QJsonDocument(blob).toJson(QJsonDocument::Compact);

    QByteArray salt;
    QByteArray iv;
    QByteArray tag;
    const QByteArray ciphertext =
        encryptBlob(plaintext, m_supabaseAnonKey.toUtf8(), salt, iv, tag);

    if (ciphertext.isEmpty()) {
        emit syncCompleted(false);
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("id"), profile.id);
    payload.insert(QStringLiteral("user_id"), profile.id);
    payload.insert(QStringLiteral("username"), profile.username);
    payload.insert(QStringLiteral("email"), profile.email);
    payload.insert(QStringLiteral("encrypted_blob"),
                   QString::fromLatin1(ciphertext.toBase64()));
    payload.insert(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));
    payload.insert(QStringLiteral("iv"), QString::fromLatin1(iv.toBase64()));
    payload.insert(QStringLiteral("tag"), QString::fromLatin1(tag.toBase64()));
    payload.insert(QStringLiteral("updated_at"),
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QNetworkRequest request(QUrl(m_supabaseUrl + QStringLiteral("/rest/v1/profiles")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("apikey", m_supabaseAnonKey.toUtf8());
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer ").toUtf8() + m_accessToken.toUtf8());
    request.setRawHeader("Prefer", "resolution=merge-duplicates");

    QNetworkReply* reply =
        m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (!ok) {
            qWarning() << "AccountManager: cloud sync failed:"
                       << reply->errorString();
        }
        emit syncCompleted(ok);
    });
}
