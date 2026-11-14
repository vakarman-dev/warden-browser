#include "FirstRunWizard.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

const char* kTheme = R"(
QDialog#FirstRunWizard {
    background-color: #121417;
}
QLabel { color: #e6e9ef; }
QLabel#titleLabel { font-size: 22px; font-weight: 600; color: #ffffff; }
QLabel#subtitleLabel { color: #8b93a1; font-size: 13px; }
QLabel#cardTitle { font-size: 16px; font-weight: 600; color: #ffffff; }
QLabel#cardSubtitle { color: #8b93a1; font-size: 13px; }
QLabel#pathLabel { color: #6b7280; font-size: 12px; }
QLabel#errorLabel { color: #e57373; font-size: 13px; }
QLineEdit {
    background-color: #1b1f24;
    border: 1px solid #2a3038;
    border-radius: 8px;
    padding: 9px 12px;
    color: #e6e9ef;
    selection-background-color: #4a90e2;
}
QLineEdit:focus { border: 1px solid #4a90e2; }
QCheckBox, QRadioButton { color: #c7cdd6; spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1px solid #3a4450; border-radius: 4px;
    background: #1b1f24;
}
QCheckBox::indicator:checked { background: #4a90e2; border: 1px solid #4a90e2; }
QRadioButton::indicator {
    width: 16px; height: 16px;
    border: 1px solid #3a4450; border-radius: 9px;
    background: #1b1f24;
}
QRadioButton::indicator:checked { border: 4px solid #4a90e2; background: #ffffff; }
QPushButton {
    background-color: #1b1f24;
    border: 1px solid #2a3038;
    border-radius: 8px;
    padding: 9px 18px;
    color: #e6e9ef;
}
QPushButton:hover { border-color: #4a90e2; }
QPushButton#primaryButton {
    background-color: #4a90e2;
    border: none;
    color: #ffffff;
}
QPushButton#primaryButton:hover { background-color: #5c9ce8; }
QPushButton#ghostButton { background: transparent; border: 1px solid #3a4450; }
QTabWidget::pane {
    border: 1px solid #2a3038;
    border-radius: 8px;
    background: #1b1f24;
}
QTabBar::tab {
    background: transparent; color: #8b93a1;
    padding: 8px 16px; border: none;
}
QTabBar::tab:selected { color: #ffffff; border-bottom: 2px solid #4a90e2; }
QFrame#card {
    background-color: #1b1f24;
    border: 1px solid #2a3038;
    border-radius: 12px;
}
)";

QStringList browserProfilePaths(const QString& browser)
{
    QStringList result;
#if defined(Q_OS_WIN)
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QString appData = qEnvironmentVariable("APPDATA");
    if (browser == "chrome")
        result << (localAppData + "/Google/Chrome/User Data/Default/Bookmarks");
    else if (browser == "edge")
        result << (localAppData + "/Microsoft/Edge/User Data/Default/Bookmarks");
    else if (browser == "brave")
        result << (localAppData + "/BraveSoftware/Brave-Browser/User Data/Default/Bookmarks");
    else if (browser == "firefox")
        result << (appData + "/Mozilla/Firefox/Profiles");
#elif defined(Q_OS_MACOS)
    const QString home = QDir::homePath();
    if (browser == "chrome")
        result << (home + "/Library/Application Support/Google/Chrome/Default/Bookmarks");
    else if (browser == "edge")
        result << (home + "/Library/Application Support/Microsoft Edge/Default/Bookmarks");
    else if (browser == "brave")
        result << (home + "/Library/Application Support/BraveSoftware/Brave-Browser/Default/Bookmarks");
    else if (browser == "firefox")
        result << (home + "/Library/Application Support/Firefox/Profiles");
#else
    const QString home = QDir::homePath();
    if (browser == "chrome")
        result << (home + "/.config/google-chrome/Default/Bookmarks");
    else if (browser == "edge")
        result << (home + "/.config/microsoft-edge/Default/Bookmarks");
    else if (browser == "brave")
        result << (home + "/.config/BraveSoftware/Brave-Browser/Default/Bookmarks");
    else if (browser == "firefox")
        result << (home + "/.mozilla/firefox");
#endif
    return result;
}

QString findFirefoxProfile(const QString& baseDir)
{
    QDir base(baseDir);
    if (!base.exists())
        return QString();

    if (base.exists(QStringLiteral("places.sqlite")))
        return base.absolutePath();

    const QStringList entries = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        QDir dir(base.filePath(entry));
        if (dir.exists(QStringLiteral("places.sqlite")))
            return dir.absolutePath();
    }
    return QString();
}

void extractChromiumUrls(const QJsonObject& node, QStringList& out)
{
    if (node.value(QStringLiteral("type")).toString() == QStringLiteral("url")) {
        const QString url = node.value(QStringLiteral("url")).toString();
        if (!url.isEmpty())
            out << url;
    }
    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& child : children)
        extractChromiumUrls(child.toObject(), out);
}

QStringList parseChromiumBookmarks(const QString& path)
{
    QStringList urls;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return urls;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return urls;

    const QJsonObject roots = doc.object().value(QStringLiteral("roots")).toObject();
    for (auto it = roots.begin(); it != roots.end(); ++it)
        extractChromiumUrls(it.value().toObject(), urls);
    return urls;
}

void extractFirefoxUrls(const QJsonArray& children, QStringList& out)
{
    for (const QJsonValue& value : children) {
        const QJsonObject obj = value.toObject();
        const QString uri = obj.value(QStringLiteral("uri")).toString();
        if (!uri.isEmpty())
            out << uri;
        extractFirefoxUrls(obj.value(QStringLiteral("children")).toArray(), out);
    }
}

QStringList parseFirefoxBookmarks(const QString& profileDir)
{
    QStringList urls;
    QDir backups(profileDir + QStringLiteral("/bookmarkbackups"));
    if (!backups.exists())
        return urls;

    const QStringList files = backups.entryList(
        { QStringLiteral("bookmarks-*.json") }, QDir::Files, QDir::Time);
    if (files.isEmpty())
        return urls;

    QFile file(backups.filePath(files.first()));
    if (!file.open(QIODevice::ReadOnly))
        return urls;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return urls;

    extractFirefoxUrls(doc.object().value(QStringLiteral("children")).toArray(), urls);
    return urls;
}

} // namespace

FirstRunWizard::FirstRunWizard(AccountManager& accountManager, QWidget* parent)
    : QDialog(parent)
    , m_accountManager(accountManager)
{
    setObjectName(QStringLiteral("FirstRunWizard"));
    setWindowTitle(tr("Настройка Warden Browser"));
    setModal(true);
    setMinimumSize(560, 620);
    setStyleSheet(QString::fromLatin1(kTheme));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(28, 24, 28, 20);
    rootLayout->setSpacing(16);

    auto* title = new QLabel(tr("Warden Browser"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    rootLayout->addWidget(title);

    auto* subtitle = new QLabel(tr("Настройка приватного браузера"), this);
    subtitle->setObjectName(QStringLiteral("subtitleLabel"));
    rootLayout->addWidget(subtitle);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildImportPage());
    m_stack->addWidget(buildAccountPage());
    m_stack->addWidget(buildSearchPage());
    rootLayout->addWidget(m_stack, 1);

    auto* navLayout = new QHBoxLayout();
    m_backButton = new QPushButton(tr("Назад"), this);
    m_backButton->setObjectName(QStringLiteral("ghostButton"));
    m_nextButton = new QPushButton(tr("Далее"), this);
    m_nextButton->setObjectName(QStringLiteral("primaryButton"));
    m_finishButton = new QPushButton(tr("Завершить настройку"), this);
    m_finishButton->setObjectName(QStringLiteral("primaryButton"));

    navLayout->addWidget(m_backButton);
    navLayout->addStretch(1);
    navLayout->addWidget(m_nextButton);
    navLayout->addWidget(m_finishButton);
    rootLayout->addLayout(navLayout);

    connect(m_backButton, &QPushButton::clicked, this, &FirstRunWizard::onBackClicked);
    connect(m_nextButton, &QPushButton::clicked, this, &FirstRunWizard::onNextClicked);
    connect(m_finishButton, &QPushButton::clicked, this, &FirstRunWizard::onFinishClicked);

    connect(&m_accountManager, &AccountManager::authSuccess, this, &FirstRunWizard::onAuthSuccess);
    connect(&m_accountManager, &AccountManager::authError, this, &FirstRunWizard::onAuthError);

    updateNavButtons();
}

QStringList FirstRunWizard::importedBookmarks() const
{
    return m_importedBookmarks;
}

UserProfile FirstRunWizard::currentUser() const
{
    return m_currentProfile;
}

QWidget* FirstRunWizard::buildImportPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(14);

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("card"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(10);

    auto* heading = new QLabel(tr("Импорт данных из других браузеров"), card);
    heading->setObjectName(QStringLiteral("cardTitle"));

    auto* note = new QLabel(
        tr("Перенесём закладки из найденных профилей. "
           "Все данные остаются на вашем устройстве."), card);
    note->setObjectName(QStringLiteral("cardSubtitle"));
    note->setWordWrap(true);

    cardLayout->addWidget(heading);
    cardLayout->addWidget(note);
    cardLayout->addSpacing(6);

    auto addBrowserRow = [&](QCheckBox*& box, const QString& name, const QString& browserId) {
        box = new QCheckBox(name, card);
        box->setChecked(false);

        const QStringList paths = browserProfilePaths(browserId);
        bool found = false;
        for (const QString& path : paths) {
            if (browserId == QStringLiteral("firefox")) {
                if (!findFirefoxProfile(path).isEmpty()) { found = true; break; }
            } else if (QFileInfo::exists(path)) {
                found = true;
                break;
            }
        }

        auto* status = new QLabel(card);
        status->setObjectName(QStringLiteral("pathLabel"));
        if (found) {
            status->setText(tr("Профиль найден"));
            box->setChecked(true);
        } else {
            status->setText(tr("Профиль не найден"));
            box->setEnabled(false);
        }

        cardLayout->addWidget(box);
        cardLayout->addWidget(status);
    };

    addBrowserRow(m_chromeBox, tr("Google Chrome"), QStringLiteral("chrome"));
    addBrowserRow(m_firefoxBox, tr("Mozilla Firefox"), QStringLiteral("firefox"));
    addBrowserRow(m_edgeBox, tr("Microsoft Edge"), QStringLiteral("edge"));
    addBrowserRow(m_braveBox, tr("Brave"), QStringLiteral("brave"));

    layout->addWidget(card);
    layout->addStretch(1);
    return page;
}

QWidget* FirstRunWizard::buildAccountPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(14);

    m_accountTabs = new QTabWidget(page);

    // --- Вкладка «Создать аккаунт» ---
    auto* createTab = new QWidget(m_accountTabs);
    auto* createLayout = new QVBoxLayout(createTab);
    createLayout->setContentsMargins(20, 20, 20, 20);
    createLayout->setSpacing(10);

    auto* nickLabel = new QLabel(tr("Никнейм"), createTab);
    m_nicknameEdit = new QLineEdit(createTab);
    m_nicknameEdit->setPlaceholderText(tr("Например, warden_user"));

    auto* emailCreateLabel = new QLabel(tr("Email"), createTab);
    m_emailCreateEdit = new QLineEdit(createTab);
    m_emailCreateEdit->setPlaceholderText(tr("you@example.com"));

    auto* passCreateLabel = new QLabel(tr("Пароль"), createTab);
    m_passwordCreateEdit = new QLineEdit(createTab);
    m_passwordCreateEdit->setEchoMode(QLineEdit::Password);
    m_passwordCreateEdit->setPlaceholderText(tr("Минимум 8 символов"));

    m_syncBox = new QCheckBox(tr("Включить облачную синхронизацию E2EE"), createTab);
    m_syncBox->setChecked(true);

    auto* createButton = new QPushButton(tr("Создать аккаунт"), createTab);
    createButton->setObjectName(QStringLiteral("primaryButton"));

    createLayout->addWidget(nickLabel);
    createLayout->addWidget(m_nicknameEdit);
    createLayout->addWidget(emailCreateLabel);
    createLayout->addWidget(m_emailCreateEdit);
    createLayout->addWidget(passCreateLabel);
    createLayout->addWidget(m_passwordCreateEdit);
    createLayout->addWidget(m_syncBox);
    createLayout->addSpacing(8);
    createLayout->addWidget(createButton);
    createLayout->addStretch(1);

    connect(createButton, &QPushButton::clicked, this, &FirstRunWizard::onCreateAccountClicked);

    // --- Вкладка «Войти в аккаунт» ---
    auto* loginTab = new QWidget(m_accountTabs);
    auto* loginLayout = new QVBoxLayout(loginTab);
    loginLayout->setContentsMargins(20, 20, 20, 20);
    loginLayout->setSpacing(10);

    auto* emailLoginLabel = new QLabel(tr("Email"), loginTab);
    m_emailLoginEdit = new QLineEdit(loginTab);
    m_emailLoginEdit->setPlaceholderText(tr("you@example.com"));

    auto* passLoginLabel = new QLabel(tr("Пароль"), loginTab);
    m_passwordLoginEdit = new QLineEdit(loginTab);
    m_passwordLoginEdit->setEchoMode(QLineEdit::Password);
    m_passwordLoginEdit->setPlaceholderText(tr("Ваш пароль"));

    auto* loginButton = new QPushButton(tr("Войти"), loginTab);
    loginButton->setObjectName(QStringLiteral("primaryButton"));

    loginLayout->addWidget(emailLoginLabel);
    loginLayout->addWidget(m_emailLoginEdit);
    loginLayout->addWidget(passLoginLabel);
    loginLayout->addWidget(m_passwordLoginEdit);
    loginLayout->addSpacing(8);
    loginLayout->addWidget(loginButton);
    loginLayout->addStretch(1);

    connect(loginButton, &QPushButton::clicked, this, &FirstRunWizard::onLoginClicked);

    m_accountTabs->addTab(createTab, tr("Создать аккаунт"));
    m_accountTabs->addTab(loginTab, tr("Войти в аккаунт"));

    layout->addWidget(m_accountTabs, 1);

    m_authErrorLabel = new QLabel(page);
    m_authErrorLabel->setObjectName(QStringLiteral("errorLabel"));
    m_authErrorLabel->setWordWrap(true);
    m_authErrorLabel->setVisible(false);
    layout->addWidget(m_authErrorLabel);

    auto* guestButton = new QPushButton(tr("Продолжить как гость"), page);
    guestButton->setObjectName(QStringLiteral("ghostButton"));
    layout->addWidget(guestButton);

    connect(guestButton, &QPushButton::clicked, this, &FirstRunWizard::onGuestClicked);

    return page;
}

QWidget* FirstRunWizard::buildSearchPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(14);

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("card"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(12);

    auto* heading = new QLabel(tr("Выберите поисковую систему по умолчанию"), card);
    heading->setObjectName(QStringLiteral("cardTitle"));

    auto* note = new QLabel(tr("Рекомендуем DuckDuckGo — поиск без отслеживания."), card);
    note->setObjectName(QStringLiteral("cardSubtitle"));
    note->setWordWrap(true);

    m_duckDuckGoRadio = new QRadioButton(tr("DuckDuckGo — приватный поиск"), card);
    m_googleRadio = new QRadioButton(tr("Google"), card);
    m_yandexRadio = new QRadioButton(tr("Яндекс"), card);
    m_duckDuckGoRadio->setChecked(true);

    auto* group = new QButtonGroup(card);
    group->addButton(m_duckDuckGoRadio);
    group->addButton(m_googleRadio);
    group->addButton(m_yandexRadio);

    cardLayout->addWidget(heading);
    cardLayout->addWidget(note);
    cardLayout->addSpacing(6);
    cardLayout->addWidget(m_duckDuckGoRadio);
    cardLayout->addWidget(m_googleRadio);
    cardLayout->addWidget(m_yandexRadio);

    layout->addWidget(card);
    layout->addStretch(1);
    return page;
}

void FirstRunWizard::collectImportSelections()
{
    m_importedBookmarks.clear();

    auto importChromium = [this](QCheckBox* box, const QString& browserId) {
        if (!box || !box->isChecked())
            return;
        const QStringList paths = browserProfilePaths(browserId);
        for (const QString& path : paths) {
            if (QFileInfo::exists(path)) {
                m_importedBookmarks += parseChromiumBookmarks(path);
                break;
            }
        }
    };

    importChromium(m_chromeBox, QStringLiteral("chrome"));
    importChromium(m_edgeBox, QStringLiteral("edge"));
    importChromium(m_braveBox, QStringLiteral("brave"));

    if (m_firefoxBox && m_firefoxBox->isChecked()) {
        const QStringList bases = browserProfilePaths(QStringLiteral("firefox"));
        for (const QString& base : bases) {
            const QString profile = findFirefoxProfile(base);
            if (!profile.isEmpty()) {
                m_importedBookmarks += parseFirefoxBookmarks(profile);
                break;
            }
        }
    }

    m_importedBookmarks.removeDuplicates();
}

void FirstRunWizard::goToPage(int index)
{
    m_stack->setCurrentIndex(index);
    updateNavButtons();
}

void FirstRunWizard::updateNavButtons()
{
    const int index = m_stack->currentIndex();
    m_backButton->setVisible(index > 0);
    m_nextButton->setVisible(index == 0);
    m_finishButton->setVisible(index == 2);
}

void FirstRunWizard::onNextClicked()
{
    collectImportSelections();
    goToPage(1);
}

void FirstRunWizard::onBackClicked()
{
    goToPage(m_stack->currentIndex() - 1);
}

void FirstRunWizard::onFinishClicked()
{
    QString engine = QStringLiteral("duckduckgo");
    if (m_googleRadio->isChecked())
        engine = QStringLiteral("google");
    else if (m_yandexRadio->isChecked())
        engine = QStringLiteral("yandex");

    m_accountManager.setSearchEngine(engine);
    accept();
}

void FirstRunWizard::onCreateAccountClicked()
{
    const QString nickname = m_nicknameEdit->text().trimmed();
    const QString email = m_emailCreateEdit->text().trimmed();
    const QString password = m_passwordCreateEdit->text();

    if (nickname.isEmpty() || email.isEmpty() || password.isEmpty()) {
        m_authErrorLabel->setText(tr("Заполните все поля."));
        m_authErrorLabel->setVisible(true);
        return;
    }
    if (!email.contains(QLatin1Char('@')) || !email.contains(QLatin1Char('.'))) {
        m_authErrorLabel->setText(tr("Введите корректный адрес электронной почты."));
        m_authErrorLabel->setVisible(true);
        return;
    }
    if (password.size() < 8) {
        m_authErrorLabel->setText(tr("Пароль должен содержать не менее 8 символов."));
        m_authErrorLabel->setVisible(true);
        return;
    }

    m_authErrorLabel->setVisible(false);

    const bool ok = m_accountManager.registerUser(nickname, email, password, m_syncBox->isChecked());
    if (!ok) {
        m_authErrorLabel->setText(tr("Не удалось создать аккаунт. Проверьте данные и повторите попытку."));
        m_authErrorLabel->setVisible(true);
    }
}

void FirstRunWizard::onLoginClicked()
{
    const QString email = m_emailLoginEdit->text().trimmed();
    const QString password = m_passwordLoginEdit->text();

    if (email.isEmpty() || password.isEmpty()) {
        m_authErrorLabel->setText(tr("Введите email и пароль."));
        m_authErrorLabel->setVisible(true);
        return;
    }

    m_authErrorLabel->setVisible(false);

    const bool ok = m_accountManager.loginUser(email, password);
    if (!ok) {
        m_authErrorLabel->setText(tr("Не удалось войти. Проверьте email и пароль."));
        m_authErrorLabel->setVisible(true);
    }
}

void FirstRunWizard::onGuestClicked()
{
    m_authErrorLabel->setVisible(false);

    const bool ok = m_accountManager.loginAsGuest();
    if (!ok) {
        m_authErrorLabel->setText(tr("Не удалось продолжить как гость."));
        m_authErrorLabel->setVisible(true);
    }
}

void FirstRunWizard::onAuthSuccess(const UserProfile& profile)
{
    m_currentProfile = profile;
    goToPage(2);
}

void FirstRunWizard::onAuthError(const QString& message)
{
    m_authErrorLabel->setText(message);
    m_authErrorLabel->setVisible(true);
}
