#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QDebug>

#include <memory>

#include "AccountManager.h"
#include "MainWindow.h"
#include "FirstRunWizard.h"

namespace {

constexpr auto kAppName        = "Warden Browser";
constexpr auto kAppVersion     = "0.1.0";
constexpr auto kOrganization   = "vakarman-dev";
constexpr auto kOrganizationDomain = "vakarman.dev";
constexpr auto kFirstRunKey    = "first_run_completed";

void configureApplicationMetadata()
{
    QCoreApplication::setApplicationName(QString::fromLatin1(kAppName));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(kAppVersion));
    QCoreApplication::setOrganizationName(QString::fromLatin1(kOrganization));
    QCoreApplication::setOrganizationDomain(QString::fromLatin1(kOrganizationDomain));
}

void applyDarkFusionPalette(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;

    const QColor window(32, 32, 32);
    const QColor windowText(222, 222, 222);
    const QColor base(24, 24, 24);
    const QColor alternateBase(40, 40, 40);
    const QColor text(222, 222, 222);
    const QColor button(42, 42, 42);
    const QColor buttonText(222, 222, 222);
    const QColor bright(255, 255, 255);
    const QColor highlight(42, 130, 218);
    const QColor highlightedText(255, 255, 255);
    const QColor toolTipBase(50, 50, 50);
    const QColor toolTipText(222, 222, 222);
    const QColor placeholder(140, 140, 140);
    const QColor disabled(90, 90, 90);

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, windowText);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternateBase);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, buttonText);
    palette.setColor(QPalette::BrightText, bright);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, highlightedText);
    palette.setColor(QPalette::ToolTipBase, toolTipBase);
    palette.setColor(QPalette::ToolTipText, toolTipText);
    palette.setColor(QPalette::PlaceholderText, placeholder);

    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(70, 70, 70));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);

    app.setPalette(palette);
}

QString ensureDataDirectories()
{
    const QString dataLocation =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    const QStringList subdirs = {
        QStringLiteral("profile"),
        QStringLiteral("cache"),
        QStringLiteral("downloads"),
        QStringLiteral("database"),
        QStringLiteral("extensions"),
    };

    for (const QString& sub : subdirs) {
        const QString path = dataLocation + QDir::separator() + sub;
        if (!QDir().mkpath(path)) {
            qWarning() << "Failed to create data directory:" << path;
        }
    }

    return dataLocation;
}

bool isFirstRun()
{
    QSettings settings;
    return !settings.value(QString::fromLatin1(kFirstRunKey), false).toBool();
}

void markSetupComplete()
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kFirstRunKey), true);
    settings.sync();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    configureApplicationMetadata();
    applyDarkFusionPalette(app);

    const QString dataLocation = ensureDataDirectories();
    qInfo() << "Data location:" << dataLocation;

    AccountManager accountManager;
    if (!accountManager.initialize(dataLocation)) {
        qCritical() << "Failed to initialize AccountManager.";
        return 1;
    }

    std::unique_ptr<QWidget> window;
    if (isFirstRun()) {
        auto wizard = std::make_unique<FirstRunWizard>(accountManager);
        window = std::move(wizard);
    } else {
        window = std::make_unique<MainWindow>(accountManager);
    }

    // Один раз после успешного показа главного окна считаем первый запуск завершённым.
    QObject::connect(window.get(), &QWidget::destroyed, []() {
        markSetupComplete();
    });

    window->show();

    return app.exec();
}
