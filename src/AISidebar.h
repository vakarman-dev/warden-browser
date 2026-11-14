// src/AISidebar.h
#pragma once

#include <QWidget>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QWebEngineView;

/**
 * Выдвижная боковая панель с встроенным QWebEngineView для работы
 * с бесплатными нейросетями по кругу. При обнаружении страницы
 * «лимит исчерпан / HTTP 429» автоматически переключается
 * на следующий сервис в очереди.
 */
class AISidebar : public QWidget
{
    Q_OBJECT

public:
    explicit AISidebar(QWidget* parent = nullptr);

    QString currentServiceName() const;
    QString currentServiceUrl() const;

public slots:
    // Переключиться на следующую нейросеть по кругу.
    void nextService();
    // Загрузить конкретный сервис по индексу.
    void loadService(int index);

signals:
    void serviceChanged(const QString& name);

private slots:
    void onLoadFinished(bool ok);
    void onServiceSelected(int index);

private:
    struct AIService
    {
        QString name;
        QString url;
    };

    QVector<AIService> m_services;
    int m_currentIndex = 0;

    QWebEngineView* m_view = nullptr;
    QComboBox* m_serviceCombo = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_statusLabel = nullptr;

    void buildUi();
    void scanPageForLimit();
    bool containsLimitPhrase(const QString& text) const;
};
