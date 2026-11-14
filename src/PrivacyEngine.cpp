#include "PrivacyEngine.h"

#include <QtGlobal>

#include <QNetworkProxy>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

TrackerInterceptor::TrackerInterceptor(QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    m_trackerDomains = {
        // Google / DoubleClick
        QStringLiteral("google-analytics.com"),
        QStringLiteral("googletagmanager.com"),
        QStringLiteral("doubleclick.net"),
        QStringLiteral("googleadservices.com"),
        QStringLiteral("googlesyndication.com"),
        QStringLiteral("adservice.google.com"),
        QStringLiteral("2mdn.net"),
        // Yandex Metrica
        QStringLiteral("mc.yandex.ru"),
        QStringLiteral("metrika.yandex.ru"),
        // Facebook Pixel
        QStringLiteral("connect.facebook.net"),
        QStringLiteral("facebook.net"),
        // Analytics / error-tracking suites
        QStringLiteral("hotjar.com"),
        QStringLiteral("hotjar.io"),
        QStringLiteral("mixpanel.com"),
        QStringLiteral("sentry.io"),
        QStringLiteral("newrelic.com"),
        QStringLiteral("scorecardresearch.com"),
        QStringLiteral("quantserve.com"),
        QStringLiteral("chartbeat.com"),
        QStringLiteral("crazyegg.com"),
        QStringLiteral("amplitude.com"),
        QStringLiteral("segment.io"),
        QStringLiteral("segment.com"),
        QStringLiteral("fullstory.com"),
        QStringLiteral("heap.io"),
        QStringLiteral("branch.io"),
        QStringLiteral("mouseflow.com"),
        QStringLiteral("luckyorange.com"),
        QStringLiteral("inspectlet.com"),
        // Ad networks / exchanges
        QStringLiteral("criteo.com"),
        QStringLiteral("criteo.net"),
        QStringLiteral("adnxs.com"),
        QStringLiteral("rubiconproject.com"),
        QStringLiteral("pubmatic.com"),
        QStringLiteral("openx.net"),
        QStringLiteral("taboola.com"),
        QStringLiteral("outbrain.com"),
        QStringLiteral("amazon-adsystem.com"),
        QStringLiteral("adsrvr.org"),
        QStringLiteral("teads.tv"),
        QStringLiteral("smartadserver.com"),
        QStringLiteral("moatads.com"),
        QStringLiteral("adcolony.com")
    };
}

void TrackerInterceptor::setHttpsOnly(bool enabled)
{
    if (m_httpsOnly == enabled)
        return;
    m_httpsOnly = enabled;
}

bool TrackerInterceptor::httpsOnly() const
{
    return m_httpsOnly;
}

void TrackerInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    const QUrl url = info.requestUrl();
    const QString host = url.host().toLower();

    if (isTrackerHost(host, m_trackerDomains)) {
        info.block(true);
        emit trackerBlocked(url.toString());
        return;
    }

    // Удаляем межсайтовый Referer, чтобы не отдавать источник перехода
    // третьим сторонам.
    info.setHttpHeader("Referer", QByteArray());

    // Режим "только HTTPS": небезопасные http:// ресурсы (кроме localhost)
    // автоматически перенаправляются на https://.
    if (m_httpsOnly
        && url.scheme() == QStringLiteral("http")
        && !isLocalHost(host)) {
        QUrl secureUrl = url;
        secureUrl.setScheme(QStringLiteral("https"));
        if (secureUrl.port() == 80)
            secureUrl.setPort(-1);
        info.redirect(secureUrl);
    }
}

bool TrackerInterceptor::isTrackerHost(const QString& host, const QSet<QString>& trackerDomains)
{
    if (host.isEmpty())
        return false;

    for (const QString& domain : trackerDomains) {
        if (host == domain || host.endsWith(QStringLiteral(".") + domain))
            return true;
    }
    return false;
}

bool TrackerInterceptor::isLocalHost(const QString& host)
{
    return host == QStringLiteral("localhost")
        || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1")
        || host.endsWith(QStringLiteral(".localhost"));
}

PrivacyEngine::PrivacyEngine(QObject* parent)
    : QObject(parent)
    , m_interceptor(new TrackerInterceptor(this))
{
    connect(m_interceptor, &TrackerInterceptor::trackerBlocked,
            this, [this](const QString&) {
                ++m_blockedCount;
                emit blockedCountChanged(m_blockedCount);
            });
}

PrivacyEngine::~PrivacyEngine() = default;

void PrivacyEngine::attachToProfile(QWebEngineProfile* profile)
{
    if (!profile)
        return;

    // Подключаем перехватчик запросов.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    profile->setRequestInterceptor(m_interceptor);
#else
    profile->setUrlRequestInterceptor(m_interceptor);
#endif

    QWebEngineSettings* settings = profile->settings();

    // Отключаем утечку реального IP через WebRTC: разрешаем только
    // проксируемый UDP, без прямых (не проксируемых) соединений.
    settings->setAttribute(QWebEngineSettings::WebRTCIPHandlingPolicy,
                           QWebEngineSettings::DisableNonProxiedUdp);

    // Запрещаем постоянные (persistent) куки — сессия живёт только в памяти.
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    // Дополнительная защита: отключаем DNS-prefetch (предотвращает утечки DNS)
    // и доступ JavaScript к буферу обмена.
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);

    // Чистый, обезличенный User-Agent.
    profile->setHttpUserAgent(QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/126.0.0.0 Safari/537.36"));
}

void PrivacyEngine::setHttpsOnly(bool enabled)
{
    m_interceptor->setHttpsOnly(enabled);
}

bool PrivacyEngine::httpsOnly() const
{
    return m_interceptor->httpsOnly();
}

void PrivacyEngine::setTorEnabled(bool enabled, const QString& host, quint16 port)
{
    m_torHost = host;
    m_torPort = port;

    if (enabled) {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(host);
        proxy.setPort(port);
        QNetworkProxy::setApplicationProxy(proxy);
    } else {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }

    m_torEnabled = enabled;
    emit torStatusChanged(enabled);
}

bool PrivacyEngine::isTorEnabled() const
{
    return m_torEnabled;
}

int PrivacyEngine::blockedCount() const
{
    return m_blockedCount;
}

void PrivacyEngine::burnSession(QWebEngineProfile* profile)
{
    if (!profile)
        return;

    // Мгновенно стираем все следы текущей сессии.
    profile->cookieStore()->deleteAllCookies();
    profile->clearHttpCache();
    profile->clearAllVisitedLinks();

    m_blockedCount = 0;
    emit blockedCountChanged(0);
    emit sessionBurned();
}
