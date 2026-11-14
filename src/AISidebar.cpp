#include "AISidebar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {

// Фразы, характерные для страниц «лимит исчерпан» и ошибки HTTP 429.
const QStringList kLimitPhrases = {
    QStringLiteral("rate limit"),
    QStringLiteral("too many requests"),
    QStringLiteral("429"),
    QStringLiteral("http error 429"),
    QStringLiteral("usage limit"),
    QStringLiteral("limit reached"),
    QStringLiteral("you've reached"),
    QStringLiteral("you have reached"),
    QStringLiteral("try again later"),
    QStringLiteral("slow down"),
    QStringLiteral("превышен лимит"),
    QStringLiteral("превышено число запросов"),
    QStringLiteral("лимит запросов"),
    QStringLiteral("слишком много запросов"),
    QStringLiteral("попробуйте позже"),
};

} // namespace

AISidebar::AISidebar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("aiSidebar"));
    setMinimumWidth(300);
    setMaximumWidth(560);

    // Очередь бесплатных ИИ-сервисов (URL по ТЗ Модуля 5).
    // При желании можно углубиться сразу в чат:
    //   DuckDuckGo AI Chat -> https://duckduckgo.com/?q=chat&ia=chat
    //   HuggingChat        -> https://huggingface.co/chat
    m_services = {
        { QStringLiteral("ChatGPT"),            QStringLiteral("https://chatgpt.com") },
        { QStringLiteral("DuckDuckGo AI Chat"), QStringLiteral("https://duckduckgo.com") },
        { QStringLiteral("HuggingChat"),        QStringLiteral("https://huggingface.co") },
    };

    buildUi();
    loadService(0);
}

void AISidebar::buildUi()
{
    m_serviceCombo = new QComboBox(this);
    for (const AIService& service : m_services)
        m_serviceCombo->addItem(service.name);

    m_nextButton = new QPushButton(tr("Следующий ИИ"), this);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("aiStatus"));
    m_statusLabel->setWordWrap(true);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(0, 0, 0, 0);
    topBar->setSpacing(6);
    topBar->addWidget(m_serviceCombo, 1);
    topBar->addWidget(m_nextButton);

    m_view = new QWebEngineView(this);
    QWebEngineSettings* settings = m_view->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addLayout(topBar);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_view, 1);

    setStyleSheet(QStringLiteral(
        "#aiSidebar { background: #1b1f24; border-left: 1px solid #2c323b; }"
        "QLabel#aiStatus { color: #9aa4b2; font-size: 11px; }"
        "QComboBox, QPushButton { background: #121417; color: #e6e9ee;"
        "  border: 1px solid #2c323b; border-radius: 6px; padding: 4px 8px; }"
        "QComboBox QAbstractItemView { background: #121417; color: #e6e9ee;"
        "  selection-background-color: #4a90e2; }"
        "QPushButton:hover { border-color: #4a90e2; }"));

    connect(m_serviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AISidebar::onServiceSelected);
    connect(m_nextButton, &QPushButton::clicked, this, &AISidebar::nextService);
    connect(m_view, &QWebEngineView::loadFinished, this, &AISidebar::onLoadFinished);
}

QString AISidebar::currentServiceName() const
{
    return m_services.value(m_currentIndex).name;
}

QString AISidebar::currentServiceUrl() const
{
    return m_services.value(m_currentIndex).url;
}

void AISidebar::nextService()
{
    if (m_services.isEmpty())
        return;
    loadService((m_currentIndex + 1) % int(m_services.size()));
}

void AISidebar::loadService(int index)
{
    if (m_services.isEmpty())
        return;
    if (index < 0 || index >= int(m_services.size()))
        index = 0;

    m_currentIndex = index;
    const AIService& service = m_services.at(m_currentIndex);

    if (m_serviceCombo) {
        const QSignalBlocker blocker(m_serviceCombo);
        m_serviceCombo->setCurrentIndex(m_currentIndex);
    }
    if (m_statusLabel)
        m_statusLabel->setText(QStringLiteral("%1 — загрузка…").arg(service.name));
    if (m_view)
        m_view->load(QUrl(service.url));

    emit serviceChanged(service.name);
}

void AISidebar::onServiceSelected(int index)
{
    loadService(index);
}

void AISidebar::onLoadFinished(bool ok)
{
    if (m_statusLabel) {
        m_statusLabel->setText(ok
            ? QStringLiteral("%1 — готов").arg(currentServiceName())
            : QStringLiteral("%1 — ошибка загрузки").arg(currentServiceName()));
    }

    // Проверяем страницу на «лимит / 429» и в успешном, и в ошибочном
    // случае: страница ошибки тоже может содержать текст вида «429».
    scanPageForLimit();
}

void AISidebar::scanPageForLimit()
{
    if (!m_view || !m_view->page())
        return;

    QPointer<AISidebar> guard(this);
    m_view->page()->runJavaScript(
        QStringLiteral(
            "(function () {"
            "  try { return document.body ? document.body.innerText : ''; }"
            "  catch (e) { return ''; }"
            "})()"),
        [guard](const QVariant& result) {
            if (!guard)
                return;

            const QString text = result.toString();
            if (guard->containsLimitPhrase(text)) {
                if (guard->m_statusLabel)
                    guard->m_statusLabel->setText(
                        QStringLiteral("Обнаружен лимит/429 — переключаюсь…"));
                guard->nextService();
            }
        });
}

bool AISidebar::containsLimitPhrase(const QString& text) const
{
    if (text.isEmpty())
        return false;

    const QString haystack = text.toLower().simplified();
    for (const QString& phrase : kLimitPhrases) {
        if (haystack.contains(phrase))
            return true;
    }
    return false;
}
