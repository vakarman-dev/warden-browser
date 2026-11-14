#pragma once

#include <QMainWindow>

class AccountManager;
class PrivacyEngine;
class AISidebar;

class QTabWidget;
class QLineEdit;
class QLabel;
class QToolButton;
class QPushButton;
class QWebEngineView;
class QWebEngineProfile;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AccountManager& accountManager, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void createTab(const QString& url = QString());
    void closeTab(int index);
    void onCurrentTabChanged(int index);

    void navigate();
    void goBack();
    void goForward();
    void reloadPage();
    void goHome();

    void setAISidebarVisible(bool visible);

    void updateBlockedCount(int count);
    void setTorEnabled(bool enabled);
    void burnSession();

    void updateAccountBadge();
    void updateNavButtons();

private:
    AccountManager& m_accountManager;

    PrivacyEngine* m_privacyEngine = nullptr;
    QWebEngineProfile* m_profile = nullptr;

    QTabWidget* m_tabs = nullptr;
    QToolButton* m_newTabButton = nullptr;

    QLineEdit* m_addressBar = nullptr;
    QToolButton* m_backButton = nullptr;
    QToolButton* m_forwardButton = nullptr;
    QToolButton* m_reloadButton = nullptr;
    QToolButton* m_homeButton = nullptr;

    QLabel* m_blockedLabel = nullptr;
    QToolButton* m_torButton = nullptr;
    QLabel* m_accountBadge = nullptr;
    QToolButton* m_aiButton = nullptr;
    QPushButton* m_burnButton = nullptr;

    AISidebar* m_aiSidebar = nullptr;

    void applyDarkTheme();
    void setupUi();
    void setupToolbar();
    void setupPrivacy();

    QWebEngineView* createWebView(QWidget* parent);
    QWebEngineView* currentWebView() const;
    int indexOfView(QWebEngineView* view) const;

    QUrl newTabUrl() const;
    QString searchEngineName() const;
    QString buildSearchUrl(const QString& query) const;
    bool looksLikeUrl(const QString& input) const;
};
