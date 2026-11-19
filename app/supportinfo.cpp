#include "buildinfo.h"
#include "supportinfo.h"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QHostAddress>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSysInfo>
#include <utility>

namespace {
QMutex recentLogMutex;
QByteArray recentLog;
bool recentLogTruncated = false;
constexpr int maximumLogBytes = 64 * 1024;
}

void SupportInfo::recordLog(const QString& text)
{
    QMutexLocker locker(&recentLogMutex);
    recentLog.append(text.toUtf8());
    if (recentLog.size() > maximumLogBytes) {
        recentLog.remove(0, recentLog.size() - maximumLogBytes);
        recentLogTruncated = true;
    }
}

SupportInfo::SupportInfo(QString logPath, QObject* parent)
    : QObject(parent), m_LogPath(std::move(logPath))
{
}

QString SupportInfo::redact(QString text)
{
    // A bounded log can cut either end of a certificate/key. Omit the
    // remaining fragment too, even if that sacrifices surrounding context.
    static const QRegularExpression pem(QStringLiteral("-----BEGIN [^-]+-----.*?(?:-----END [^-]+-----|\\z)"),
                                        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression pemTail(QStringLiteral("\\A.*?-----END [^-]+-----"),
                                            QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression urlCredentials(QStringLiteral("(\\b[a-z][a-z0-9+.-]*://)[^/\\s@]+@"),
                                                   QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression urlQueries(QStringLiteral(R"((\b[a-z][a-z0-9+.-]*://[^\s"'<>?]+)\?[^\s"'<>]+)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression secrets(QStringLiteral("\\b(rikey|rikeyid|clientcert|password|passwd|pin|token|access_token|refresh_token|authorization)[\"']?\\s*[:=]\\s*[^\\r\\n]+"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression homes(QStringLiteral("/(?:home|Users)/[^/\\s]+"));
    static const QRegularExpression windowsHomes(QStringLiteral("[A-Z]:[/\\\\]Users[/\\\\][^/\\\\\\r\\n]+?(?=[/\\\\]|\\r|\\n|\\z)"),
                                                 QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ipv6Candidates(QStringLiteral("(?<![\\w:])[a-fA-F0-9:]*:[a-fA-F0-9:.]+(?:%[\\w.-]+)?(?![\\w:])"));
    static const QRegularExpression addresses(QStringLiteral("\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b"));
    text.replace(pem, QStringLiteral("[certificate omitted]"));
    text.replace(pemTail, QStringLiteral("[certificate omitted]"));
    text.replace(urlQueries, QStringLiteral("\\1 [query omitted]"));
    text.replace(urlCredentials, QStringLiteral("\\1[credentials omitted]@"));
    text.replace(secrets, QStringLiteral("\\1=[omitted]"));
    text.replace(windowsHomes, QStringLiteral("[home]"));
    text.replace(homes, QStringLiteral("/[home]"));
    // Validate candidates rather than treating timestamps or colon-separated
    // diagnostics as addresses. Work backwards so offsets stay valid.
    QList<QPair<qsizetype, qsizetype>> ipv6Matches;
    auto matches = ipv6Candidates.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        auto start = match.capturedStart();
        auto length = match.capturedLength();
        while (length > 0 && text.at(start + length - 1) == QLatin1Char('.')) --length;
        const QHostAddress address(text.mid(start, length));
        if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            if (start > 0 && start + length < text.size()
                    && text.at(start - 1) == QLatin1Char('[')
                    && text.at(start + length) == QLatin1Char(']')) {
                --start;
                length += 2;
            }
            ipv6Matches.append({start, length});
        }
    }
    for (auto it = ipv6Matches.crbegin(); it != ipv6Matches.crend(); ++it) {
        text.replace(it->first, it->second, QStringLiteral("[address]"));
    }
    text.replace(addresses, QStringLiteral("[address]"));
    return text;
}

QString SupportInfo::summary() const
{
    QString result = QStringLiteral("Moonwalk %1\nSource: %2\nMoonlight engine: %3\nQt: %4\nSystem: %5 (%6)\n")
            .arg(QStringLiteral(STARLIGHT_VERSION_STR), QStringLiteral(STARLIGHT_BUILD_REVISION),
                 QStringLiteral(VERSION_STR), QString::fromLatin1(qVersion()),
                 QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    result += QStringLiteral("RTT measures network round trip. Host processing, decoding, queueing, and rendering are separate timings.\n");
    QFile log(m_LogPath);
    if (!m_LogPath.isEmpty() && log.open(QIODevice::ReadOnly)) {
        constexpr qint64 maximumBytes = 64 * 1024;
        if (log.size() > maximumBytes) {
            log.seek(log.size() - maximumBytes);
            log.readLine(); // omit a partial leading line
        }
        result += QStringLiteral("\nRecent application log (up to 64 KiB; review before sharing):\n");
        result += redact(QString::fromUtf8(log.read(maximumBytes)));
    } else {
        QByteArray snapshot;
        {
            QMutexLocker locker(&recentLogMutex);
            snapshot = recentLog;
            if (recentLogTruncated) {
                // A truncated line can lose its secret-field label. Match the
                // file-backed summary's policy of excluding that whole line.
                const auto newline = snapshot.indexOf('\n');
                snapshot = newline < 0 ? QByteArray() : snapshot.mid(newline + 1);
            }
        }
        if (!snapshot.isEmpty()) {
            result += QStringLiteral("\nRecent application log (in memory, up to 64 KiB; review before sharing):\n");
            result += redact(QString::fromUtf8(snapshot));
        } else {
            result += QStringLiteral("\nThis process has no application log. After a stream, use Copy Diagnostic Summary in the Moonwalk window.\n");
        }
    }
    return result;
}

bool SupportInfo::copySummary() const
{
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    clipboard->setText(summary());
    return true;
}
