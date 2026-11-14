#pragma once

#include <QDialog>
#include <QStringList>

#include "AccountManager.h"

class QStackedWidget;
class QTabWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QRadioButton;

class FirstRunWizard : public QDialog
{
    Q_OBJECT

public:
    explicit FirstRunWizard(AccountManager& accountManager, QWidget* parent = nullptr);

    QStringList importedBookmarks() const;
    UserProfile currentUser() const;

private slots:
    void onNextClicked();
    void onBackClicked();
    void onFinishClicked();
    void onCreateAccountClicked();
    void onLoginClicked();
    void onGuestClicked();
    void onAuthSuccess(const UserProfile& profile);
    void onAuthError(const QString& message);

private:
    QWidget* buildImportPage();
    QWidget* buildAccountPage();
    QWidget* buildSearchPage();
    void collectImportSelections();
    void goToPage(int index);
    void updateNavButtons();

    AccountManager& m_accountManager;

    QStackedWidget* m_stack = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_finishButton = nullptr;

    // Шаг 1 — импорт
    QCheckBox* m_chromeBox = nullptr;
    QCheckBox* m_firefoxBox = nullptr;
    QCheckBox* m_edgeBox = nullptr;
    QCheckBox* m_braveBox = nullptr;

    // Шаг 2 — аккаунт
    QTabWidget* m_accountTabs = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QLineEdit* m_emailCreateEdit = nullptr;
    QLineEdit* m_passwordCreateEdit = nullptr;
    QCheckBox* m_syncBox = nullptr;
    QLineEdit* m_emailLoginEdit = nullptr;
    QLineEdit* m_passwordLoginEdit = nullptr;
    QLabel* m_authErrorLabel = nullptr;

    // Шаг 3 — поисковая система
    QRadioButton* m_duckDuckGoRadio = nullptr;
    QRadioButton* m_googleRadio = nullptr;
    QRadioButton* m_yandexRadio = nullptr;

    QStringList m_importedBookmarks;
    UserProfile m_currentProfile;
};
