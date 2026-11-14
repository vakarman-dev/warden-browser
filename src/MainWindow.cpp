#include "MainWindow.h"

#include "AccountManager.h"
#include "PrivacyEngine.h"
#include "AISidebar.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

MainWindow::MainWindow(AccountManager& accountManager, QWidget* parent)
    : QMainWindow(parent)
    , m_accountManager(accountManager)
{
    setWindowTitle(tr("Warden Browser"));
    resize(1280, 800);

    applyDarkTheme();
    setupPrivacy();
    setupUi();
    setupToolbar();

    updateAccountBadge();
    createTab();
}

MainWindow::~MainWindow() = default;

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(R"(
QMainWindow, QWidget {
    background: #121417;
    color: #e6e9ee;
}
QTabWidget::pane {
    border: none;
    background: #121417;
}
QTabBar::tab {
    background: #1b1f24;
    color: #c7cdd6;
    padding: 6px 16px;
    margin-right: 2px;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
}
QTabBar::tab:selected {
    background: #4a90e2;
    color: #ffffff;
}
QTabBar::tab:hover:!selected {
    background: #232830;
}
QLineEdit {
    background: #1b1f24;
    color: #e6e9ee;
    border: 1px solid #2c323b;
    border-radius: 8px;
    padding: 6px 12px;
    selection-background-color: #4a90e2;
}
QLineEdit:focus {
    border: 1px solid #4a90e2;
}
QToolButton {
    background: #1b1f24;
    color: #e6e9ee;
    border: 1px solid #2c323b;
    border-radius: 8px;
    padding: 6px 10px;
    margin: 2px;
}
QToolButton:hover {
    background: #232830;
    border-color: #4a90e2;
}
QToolButton:checked {
    background: #4a90e2;
    color: #ffffff;
    border-color: #4a90e2;
}
QPushButton {
    background: #1b1f24;
    color: #e6e9ee;
    border: 1px solid #2c323b;
    border-radius: 8px;
    padding: 6px 12px;
}
QPushButton:hover {
    background: #232830;
    border-color: #4a90e2;
}
QLabel {
    color: #c7cdd6;
    padding: 2px 6px;
}
QLabel#blockedLabel {
    color: #e8a33d;
}
QLabel#accountBadge {
    background: #2d3641;
    color: #cfe0ff;
    border: 1px solid #3a4550;
    border-radius: 10px;
    padding: 3px 10px;
}
QToolButton#torButton:checked {
    background: #8855c9;
    color: #ffffff;
}
QPushButton#burnButton {
    background: #b3343c;
    color: #ffffff;
    border: 1px solid #d64550;
}
QPushButton#burnButton:hover {
    background: #cb3d47;
    border-color: #ff6b73;
}
QComboBox {
    background: #1b1f24;
    color: #e6e9ee;
    border: 1px solid #2c323b;
    border-radius: 6px;
    padding: 4px 8px;
}
QComboBox QAbstractItemView {
    background: #1b1f24;
    color: #e6e9ee;
    selection-background-color: #4a90e2;
    border: 1px solid #2c323b;
}
QToolTip {
    background: #1b1f24;
    color: #e6e9ee;
    border: 1px solid #4a90e2;
}
)"));
}

void MainWindow::setupPrivacy()
{
    m_privacyEngine = new PrivacyEngine(this);
    m_profile = QWebEngineProfile::defaultProfile();
    m_privacyEngine->attachToProfile(m_profile);

    connect(m_privacyEngine, &PrivacyEngine::blockedCountChanged,
            this, &MainWindow::updateBlockedCount);
    connect(m_privacyEngine, &PrivacyEngine::torStatusChanged,
            this, &MainWindow::setTorEnabled);
    connect(m_privacyEngine, &PrivacyEngine::sessionBurned, this, [this] {
        if (m_addressBar)
            m_addressBar->clear();
        updateBlockedCount(m_privacyEngine ? m_privacyEngine->blockedCount() : 0);
    });

    updateBlockedCount(m_privacyEngine->blockedCount());
    setTorEnabled(m_privacyEngine->isTorEnabled());
}

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_tabs = new QTabWidget(central);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setUsesScrollButtons(true);

    m_aiSidebar = new AISidebar(central);
    m_aiSidebar->hide();

    rootLayout->addWidget(m_tabs, 1);
    rootLayout->addWidget(m_aiSidebar);

    setCentralWidget(central);

    m_newTabButton = new QToolButton(m_tabs);
    m_newTabButton->setText(QStringLiteral("+"));
    m_newTabButton->setToolTip(tr("Новая вкладка"));
    m_newTabButton->setAutoRaise(true);
    m_tabs->setCornerWidget(m_newTabButton, Qt::TopRightCorner);

    connect(m_newTabButton, &QToolButton::clicked, this, [this] { createTab(); });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
}

void MainWindow::setupToolbar()
{
    auto* toolbar = new QToolBar(tr("Навигация"), this);
    toolbar->setObjectName(QStringLiteral("mainToolBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    addToolBar(toolbar);

    m_backButton = new QToolButton(toolbar);
    m_backButton->setText(QStringLiteral("←"));
    m_backButton->setToolTip(tr("Назад"));
    m_backButton->setEnabled(false);
    toolbar->addWidget(m_backButton);

    m_forwardButton = new QToolButton(toolbar);
    m_forwardButton->setText(QStringLiteral("→"));
    m_forwardButton->setToolTip(tr("Вперёд"));
    m_forwardButton->setEnabled(false);
    toolbar->addWidget(m_forwardButton);

    m_reloadButton = new QToolButton(toolbar);
    m_reloadButton->setText(QStringLiteral("⟳"));
    m_reloadButton->setToolTip(tr("Обновить"));
    toolbar->addWidget(m_reloadButton);

    m_homeButton = new QToolButton(toolbar);
    m_homeButton->setText(QStringLiteral("⌂"));
    m_homeButton->setToolTip(tr("Домой"));
    toolbar->addWidget(m_homeButton);

    m_addressBar = new QLineEdit(toolbar);
    m_addressBar->setPlaceholderText(tr("Введите адрес или поисковый запрос"));
    m_addressBar->setClearButtonEnabled(true);
    m_addressBar->setMinimumWidth(320);
    toolbar->addWidget(m_addressBar);

    auto* spring = new QWidget(toolbar);
    spring->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spring);

    m_blockedLabel = new QLabel(toolbar);
    m_blockedLabel->setObjectName(QStringLiteral("blockedLabel"));
    m_blockedLabel->setText(QStringLiteral("Трекеры: 0"));
    toolbar->addWidget(m_blockedLabel);

    m_torButton = new QToolButton(toolbar);
    m_torButton->setObjectName(QStringLiteral("torButton"));
    m_torButton->setCheckable(true);
    m_torButton->setText(QStringLiteral("Tor: выкл"));
    m_torButton->setToolTip(tr("Маршрутизация через Tor (SOCKS5 127.0.0.1:9050)"));
    toolbar->addWidget(m_torButton);

    m_accountBadge = new QLabel(toolbar);
    m_accountBadge->setObjectName(QStringLiteral("accountBadge"));
    toolbar->addWidget(m_accountBadge);

    m_aiButton = new QToolButton(toolbar);
    m_aiButton->setCheckable(true);
    m_aiButton->setText(QStringLiteral("ИИ-панель"));
    m_aiButton->setToolTip(tr("Открыть/скрыть панель нейросетей"));
    toolbar->addWidget(m_aiButton);

    m_burnButton = new QPushButton(tr("Сжечь сессию"), toolbar);
    m_burnButton->setObjectName(QStringLiteral("burnButton"));
    toolbar->addWidget(m_burnButton);

    connect(m_backButton, &QToolButton::clicked, this, &MainWindow::goBack);
    connect(m_forwardButton, &QToolButton::clicked, this, &MainWindow::goForward);
    connect(m_reloadButton, &QToolButton::clicked, this, &MainWindow::reloadPage);
    connect(m_homeButton, &QToolButton::clicked, this, &MainWindow::goHome);
    connect(m_addressBar, &QLineEdit::returnPressed, this, &MainWindow::navigate);

    connect(m_torButton, &QToolButton::toggled, this, [this](bool checked) {
        if (m_privacyEngine)
            m_privacyEngine->setTorEnabled(checked);
    });
    connect(m_aiButton, &QToolButton::toggled, this, &MainWindow::setAISidebarVisible);
    connect(m_burnButton, &QPushButton::clicked, this, &MainWindow::burnSession);
}

QWebEngineView* MainWindow::createWebView(QWidget* parent)
{
    auto* view = new QWebEngineView(parent);
    view->setPage(new QWebEnginePage(m_profile, view));

    QWebEngineSettings* settings = view->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    return view;
}

QWebEngineView* MainWindow::currentWebView() const
{
    return m_tabs ? qobject_cast<QWebEngineView*>(m_tabs->currentWidget())
                  : nullptr;
}

int MainWindow::indexOfView(QWebEngineView* view) const
{
    return (m_tabs && view) ? m_tabs->indexOf(view) : -1;
}

void MainWindow::createTab(const QString& url)
{
    QWebEngineView* view = createWebView(m_tabs);
    const int index = m_tabs->addTab(view, tr("Новая вкладка"));
    m_tabs->setCurrentIndex(index);

    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString& title) {
        const int i = indexOfView(view);
        if (i != -1)
            m_tabs->setTabText(i, title.isEmpty() ? tr("Новая вкладка") : title);
    });
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl& urlValue) {
        const int i = indexOfView(view);
        if (i != -1)
            m_tabs->setTabToolTip(i, urlValue.toString());
        if (view == currentWebView()) {
            m_addressBar->setText(urlValue.toString());
            updateNavButtons();
        }
    });
    connect(view, &QWebEngineView::iconChanged, this, [this, view](const QIcon& icon) {
        const int i = indexOfView(view);
        if (i != -1)
            m_tabs->setTabIcon(i, icon);
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        Q_UNUSED(ok);
        if (view == currentWebView())
            updateNavButtons();
    });

    if (url.isEmpty())
        view->load(newTabUrl());
    else
        view->load(QUrl::fromUserInput(url));
}

void MainWindow::closeTab(int index)
{
    if (!m_tabs || index < 0 || index >= m_tabs->count())
        return;

    if (m_tabs->count() == 1) {
        // Минимум одна вкладка остаётся всегда; вместо закрытия — на главную.
        if (auto* view = currentWebView())
            view->load(newTabUrl());
        return;
    }

    QWidget* widget = m_tabs->widget(index);
    m_tabs->removeTab(index);
    if (widget) {
        widget->disconnect();
        widget->deleteLater();
    }
    updateNavButtons();
}

void MainWindow::onCurrentTabChanged(int /*index*/)
{
    if (auto* view = currentWebView())
        m_addressBar->setText(view->url().toString());
    updateNavButtons();
}

void MainWindow::navigate()
{
    if (!m_addressBar)
        return;

    const QString input = m_addressBar->text().trimmed();
    if (input.isEmpty())
        return;

    auto* view = currentWebView();
    if (!view)
        return;

    const QString target = looksLikeUrl(input)
        ? QUrl::fromUserInput(input).toString()
        : buildSearchUrl(input);

    if (!target.isEmpty()) {
        view->load(QUrl(target));
        view->setFocus();
    }
}

void MainWindow::goBack()
{
    if (auto* view = currentWebView())
        view->back();
    updateNavButtons();
}

void MainWindow::goForward()
{
    if (auto* view = currentWebView())
        view->forward();
    updateNavButtons();
}

void MainWindow::reloadPage()
{
    if (auto* view = currentWebView())
        view->reload();
}

void MainWindow::goHome()
{
    if (auto* view = currentWebView())
        view->load(newTabUrl());
    else
        createTab();
}

void MainWindow::setAISidebarVisible(bool visible)
{
    if (!m_aiSidebar)
        return;

    m_aiSidebar->setVisible(visible);

    if (m_aiButton) {
        const QSignalBlocker blocker(m_aiButton);
        m_aiButton->setChecked(visible);
    }
}

void MainWindow::updateBlockedCount(int count)
{
    if (!m_blockedLabel)
        return;
    m_blockedLabel->setText(QStringLiteral("Трекеры: %1").arg(count));
    m_blockedLabel->setToolTip(tr("Заблокировано трекеров на этой сессии: %1").arg(count));
}

void MainWindow::setTorEnabled(bool enabled)
{
    if (!m_torButton)
        return;

    const QSignalBlocker blocker(m_torButton);
    m_torButton->setChecked(enabled);
    m_torButton->setText(enabled ? QStringLiteral("Tor: вкл")
                                 : QStringLiteral("Tor: выкл"));
}

void MainWindow::burnSession()
{
    if (m_privacyEngine && m_profile)
        m_privacyEngine->burnSession(m_profile);

    if (m_tabs) {
        const int count = m_tabs->count();
        for (int i = count - 1; i >= 0; --i) {
            QWidget* widget = m_tabs->widget(i);
            m_tabs->removeTab(i);
            if (widget) {
                widget->disconnect();
                widget->deleteLater();
            }
        }
    }

    createTab(); // чистая новая вкладка
}

void MainWindow::updateAccountBadge()
{
    if (!m_accountBadge)
        return;

    const UserProfile profile = m_accountManager.currentUser();
    const QString name = profile.isGuest
        ? tr("Гость")
        : (profile.username.isEmpty() ? profile.email : profile.username);

    m_accountBadge->setText(QStringLiteral("Аккаунт: %1").arg(name));

    const QString tip = profile.isGuest
        ? tr("Гостевой режим")
        : QStringLiteral("%1 <%2>").arg(profile.username, profile.email);
    m_accountBadge->setToolTip(tip);
}

void MainWindow::updateNavButtons()
{
    auto* view = currentWebView();
    if (m_backButton)
        m_backButton->setEnabled(view && view->history()->canGoBack());
    if (m_forwardButton)
        m_forwardButton->setEnabled(view && view->history()->canGoForward());
}

QUrl MainWindow::newTabUrl() const
{
    QUrl url = QUrl::fromLocalFile(
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/newtab.html"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("engine"), searchEngineName());
    url.setQuery(query);
    return url;
}

QString MainWindow::searchEngineName() const
{
    const QString engine = m_accountManager.currentUser().searchEngine.trimmed();
    return engine.isEmpty() ? QStringLiteral("DuckDuckGo") : engine;
}

QString MainWindow::buildSearchUrl(const QString& rawQuery) const
{
    const QString engine = m_accountManager.currentUser().searchEngine.trimmed().toLower();
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(rawQuery));

    if (engine.contains(QStringLiteral("яндекс")) || engine.contains(QStringLiteral("yandex")))
        return QStringLiteral("https://yandex.ru/search/?text=") + encoded;
    if (engine.contains(QStringLiteral("google")))
        return QStringLiteral("https://www.google.com/search?q=") + encoded;

    // DuckDuckGo — поисковик по умолчанию.
    return QStringLiteral("https://duckduckgo.com/?q=") + encoded;
}

bool MainWindow::looksLikeUrl(const QString& input) const
{
    if (input.contains(QStringLiteral("://")))
        return true;
    if (input.contains(QLatin1Char(' ')))
        return false;
    if (input.startsWith(QStringLiteral("localhost"), Qt::CaseInsensitive) ||
        input.startsWith(QStringLiteral("127.0.0.1")) ||
        input.startsWith(QStringLiteral("[::1]")))
        return true;
    if (input.contains(QLatin1Char('.')))
        return true;
    return false;
}
