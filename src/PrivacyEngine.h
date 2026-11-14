#pragma once

#include <QObject>
#include <QSet>
#include <QString>

#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

class QWebEngineProfile;

class TrackerInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit TrackerInterceptor(QObject* parent = nullptr);

    void setHttpsOnly(bool enabled);
    bool httpsOnly() const;

    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

signals:
    void trackerBlocked(const QString& url);

private:
    static bool isTrackerHost(const QString& host, const QSet<QString>& trackerDomains);
    static bool isLocalHost(const QString& host);

    bool m_httpsOnly = false;
    QSet<QString> m_trackerDomains;
};

class PrivacyEngine : public QObject
{
    Q_OBJECT

public:
    explicit PrivacyEngine(QObject* parent = nullptr);
    ~PrivacyEngine() override;

    void attachToProfile(QWebEngineProfile* profile);

    void setHttpsOnly(bool enabled);
    bool httpsOnly() const;

    void setTorEnabled(bool enabled,
                       const QString& host = QStringLiteral("127.0.0.1"),
                       quint16 port = 9050);
    bool isTorEnabled() const;

    int blockedCount() const;

    void burnSession(QWebEngineProfile* profile);

signals:
    void blockedCountChanged(int count);
    void torStatusChanged(bool enabled);
    void sessionBurned();

private:
    TrackerInterceptor* m_interceptor = nullptr;
    int m_blockedCount = 0;
    bool m_torEnabled = false;
    QString m_torHost;
    quint16 m_torPort = 9050;
};
