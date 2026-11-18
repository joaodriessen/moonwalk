#include "platformprofileresolver.h"

#include <QSet>

namespace {

QStringList confirmedCapabilities(const ProfileInputs& inputs)
{
    if (inputs.hasKdeQuickControlsStyle) {
        return { QStringLiteral("kde-quick-controls-style") };
    }

    return {};
}

QSet<QString> sessionTokens(const QString& activeSession)
{
    QSet<QString> tokens;
    const QStringList values = activeSession.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString& value : values) {
        tokens.insert(value.trimmed().toLower());
    }
    return tokens;
}

bool hasPlasmaSession(const QSet<QString>& tokens)
{
    return tokens.contains(QStringLiteral("kde")) ||
            tokens.contains(QStringLiteral("plasma"));
}

bool hasGnomeSession(const QSet<QString>& tokens)
{
    return tokens.contains(QStringLiteral("gnome"));
}

} // namespace

PlatformProfile::PlatformProfile(Kind kind, QString selectionReason, QStringList confirmedCapabilities, QObject* parent)
    : QObject(parent)
    , m_Kind(kind)
    , m_SelectionReason(std::move(selectionReason))
    , m_ConfirmedCapabilities(std::move(confirmedCapabilities))
{
}

PlatformProfile::Kind PlatformProfile::kind() const
{
    return m_Kind;
}

QString PlatformProfile::name() const
{
    switch (m_Kind) {
    case Generic:
        return QStringLiteral("generic");
    case Gnome:
        return QStringLiteral("gnome");
    case Plasma:
        return QStringLiteral("plasma");
    }

    return QStringLiteral("generic");
}

QString PlatformProfile::selectionReason() const
{
    return m_SelectionReason;
}

QStringList PlatformProfile::confirmedCapabilities() const
{
    return m_ConfirmedCapabilities;
}

std::unique_ptr<PlatformProfile> PlatformProfileResolver::resolve(const ProfileInputs& inputs)
{
    const QString overrideValue = inputs.overrideValue.trimmed().toLower();
    const QStringList capabilities = confirmedCapabilities(inputs);

    if (overrideValue == QStringLiteral("generic")) {
        return std::make_unique<PlatformProfile>(PlatformProfile::Generic,
                                                 QStringLiteral("explicit override"),
                                                 capabilities);
    }
    if (overrideValue == QStringLiteral("gnome")) {
        return std::make_unique<PlatformProfile>(PlatformProfile::Gnome,
                                                 QStringLiteral("explicit override"),
                                                 capabilities);
    }
    if (overrideValue == QStringLiteral("plasma")) {
        return std::make_unique<PlatformProfile>(PlatformProfile::Plasma,
                                                 QStringLiteral("explicit override"),
                                                 capabilities);
    }
    if (!overrideValue.isEmpty()) {
        return std::make_unique<PlatformProfile>(PlatformProfile::Generic,
                                                 QStringLiteral("invalid explicit override"),
                                                 capabilities);
    }

    const QSet<QString> tokens = sessionTokens(inputs.activeSession);
    const bool gnomeSession = hasGnomeSession(tokens);
    const bool plasmaSession = hasPlasmaSession(tokens);
    if (gnomeSession && plasmaSession) {
        return std::make_unique<PlatformProfile>(
                    PlatformProfile::Generic,
                    QStringLiteral("mixed desktop session"),
                    capabilities);
    }
    if (gnomeSession) {
        return std::make_unique<PlatformProfile>(PlatformProfile::Gnome,
                                                 QStringLiteral("active GNOME session"),
                                                 capabilities);
    }
    if (plasmaSession && inputs.hasKdeQuickControlsStyle) {
        return std::make_unique<PlatformProfile>(PlatformProfile::Plasma,
                                                 QStringLiteral("active Plasma session"),
                                                 capabilities);
    }

    return std::make_unique<PlatformProfile>(PlatformProfile::Generic,
                                             QStringLiteral("generic fallback"),
                                             capabilities);
}
