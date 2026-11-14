#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QSqlDatabase>

class QNetworkAccessManager;

struct UserProfile
{
    QString id;
    QString username;
    QString email;
    QString searchEngine;
    bool isGuest = false;
    bool cloudSyncEnabled = false;
};

Q_DECLARE_METATYPE(UserProfile)

class AccountManager : public QObject
{
    Q_OBJECT

public:
    explicit AccountManager(QObject* parent = nullptr);
    ~AccountManager() override;

    bool initialize(const QString& dataLocation);
    bool hasActiveSession() const;

    bool registerUser(const QString& username,
                      const QString& email,
                      const QString& password,
                      bool enableCloudSync);

    bool loginUser(const QString& email, const QString& password);
    bool loginAsGuest();
    void logout();

    UserProfile currentUser() const;
    bool setSearchEngine(const QString& engine);

    QByteArray encryptBlob(const QByteArray& plaintext,
                           const QByteArray& password,
                           QByteArray& salt,
                           QByteArray& iv,
                           QByteArray& tag) const;

    QByteArray decryptBlob(const QByteArray& ciphertext,
                           const QByteArray& password,
                           const QByteArray& salt,
                           const QByteArray& iv,
                           const QByteArray& tag) const;

    QByteArray deriveKey(const QByteArray& password, const QByteArray& salt) const;

    void syncToCloud();

signals:
    void authSuccess(const UserProfile& profile);
    void authError(const QString& message);
    void syncCompleted(bool success);

private:
    bool loadCloudConfig();
    bool createTables();
    bool isCloudConfigured() const;

    UserProfile loadProfile(const QString& id) const;
    void setActiveProfile(const QString& id);

    void signUpWithSupabase(const QString& email,
                            const QString& password,
                            const QString& username);
    void loginWithSupabase(const QString& email, const QString& password);
    void pushProfileToCloud();

    QSqlDatabase m_db;
    QNetworkAccessManager* m_network = nullptr;
    QString m_connectionName;
    QString m_dataLocation;
    QString m_supabaseUrl;
    QString m_supabaseAnonKey;
    QString m_accessToken;
    QString m_currentProfileId;
};

#endif // ACCOUNTMANAGER_H
