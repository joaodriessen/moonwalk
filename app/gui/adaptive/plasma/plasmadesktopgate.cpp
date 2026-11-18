#include "plasmadesktopgate.h"

#include "application/adaptiveshellselection.h"

#include <QSet>
#include <QStringList>

namespace {

QSet<QString> sessionTokens(const QString& activeSession)
{
    QSet<QString> tokens;
    const QStringList values =
            activeSession.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString& value : values) {
        tokens.insert(value.trimmed().toLower());
    }
    return tokens;
}

bool isUnambiguousPlasmaSession(const QString& activeSession)
{
    const QSet<QString> tokens = sessionTokens(activeSession);
    const bool hasPlasma = tokens.contains(QStringLiteral("kde"))
            || tokens.contains(QStringLiteral("plasma"));
    if (!hasPlasma) {
        return false;
    }

    // Fail closed on every mixed or unknown identity. In particular, KDE
    // libraries are commonly co-installed under wlroots/GNOME sessions and
    // must never turn those sessions into a Plasma profile.
    static const QSet<QString> plasmaSessionTokens {
        QStringLiteral("kde"),
        QStringLiteral("plasma"),
        QStringLiteral("plasmax11"),
        QStringLiteral("plasma-x11"),
        QStringLiteral("plasmawayland"),
        QStringLiteral("plasma-wayland"),
    };
    for (const QString& token : tokens) {
        if (!plasmaSessionTokens.contains(token)) {
            return false;
        }
    }
    return true;
}

bool profileAllowsPlasma(const QString& overrideValue)
{
    const QString normalized = overrideValue.trimmed().toLower();
    return normalized.isEmpty() || normalized == QStringLiteral("plasma");
}

} // namespace

bool PlasmaDesktopGate::isNormalGuiArgumentShape(int argumentCount)
{
    // GlobalCommandLineParser's only normal-start shape is argv[0] alone.
    // Parsing occurs after application construction, so this intentionally
    // conservative pre-parser check keeps help/version and every CLI action on
    // the long-standing QGuiApplication path.
    return argumentCount == 1;
}

bool PlasmaDesktopGate::isWindowedDesktopQpa(
        const QByteArray& qpaPlatform,
        bool runningWindowManager)
{
    const QByteArray normalized = qpaPlatform.trimmed().toLower();
    if (normalized.isEmpty()) {
        return runningWindowManager;
    }

    // A fallback list can cross from a windowed desktop QPA into an excluded
    // embedded/headless backend after QApplication has already been chosen.
    // Reject every list because it cannot be predicted safely before
    // construction.
    if (normalized.contains(';')) {
        return false;
    }

    return runningWindowManager
            && (normalized == QByteArrayLiteral("xcb")
                || normalized.startsWith(QByteArrayLiteral("xcb:"))
                || normalized == QByteArrayLiteral("wayland")
                || normalized.startsWith(QByteArrayLiteral("wayland-"))
                || normalized.startsWith(QByteArrayLiteral("wayland:")));
}

bool PlasmaDesktopGate::shouldActivate(const PlasmaDesktopGateInputs& inputs)
{
    return inputs.desktopBuildSupported
            && AdaptiveShellSelection::fromOverride(
                   inputs.adaptiveShellOverride).isEnabled()
            && inputs.normalGuiRequest
            && profileAllowsPlasma(inputs.platformProfileOverride)
            && isUnambiguousPlasmaSession(inputs.activeSession)
            && inputs.styleAvailable
            && isWindowedDesktopQpa(inputs.qpaPlatform,
                                    inputs.runningWindowManager);
}
