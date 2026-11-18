#include "pollinglifecycle.h"

PollingLifecycle::PollingLifecycle(QObject* parent)
    : QObject(parent)
{
}

bool PollingLifecycle::pollingActive() const
{
    return m_PollingActive;
}

QVariantMap PollingLifecycle::visibilityChanged(bool visible, bool active)
{
    QVariantMap result = decision(visible && active);

    if (!visible) {
        result.insert(QStringLiteral("stopInactivityTimer"), true);
        if (m_PollingActive) {
            result.insert(QStringLiteral("stopPolling"), true);
        }
    }
    else if (active) {
        result.insert(QStringLiteral("stopInactivityTimer"), true);
        if (!m_PollingActive) {
            result.insert(QStringLiteral("startPolling"), true);
        }
    }

    return result;
}

QVariantMap PollingLifecycle::activityChanged(bool active, bool visible)
{
    QVariantMap result = decision(visible && active);

    if (active) {
        result.insert(QStringLiteral("stopInactivityTimer"), true);
        if (!m_PollingActive) {
            result.insert(QStringLiteral("startPolling"), true);
        }
    }
    else if (visible) {
        result.insert(QStringLiteral("startInactivityTimer"), true);
    }
    else {
        result.insert(QStringLiteral("stopInactivityTimer"), true);
    }

    return result;
}

QVariantMap PollingLifecycle::inactivityExpired(bool active)
{
    QVariantMap result = decision(false);
    if (!active && m_PollingActive) {
        result.insert(QStringLiteral("stopPolling"), true);
    }
    return result;
}

QVariantMap PollingLifecycle::decision(bool hasFocus) const
{
    return {
        { QStringLiteral("startPolling"), false },
        { QStringLiteral("stopPolling"), false },
        { QStringLiteral("startInactivityTimer"), false },
        { QStringLiteral("stopInactivityTimer"), false },
        { QStringLiteral("hasFocus"), hasFocus },
    };
}

void PollingLifecycle::commitPollingState(bool active)
{
    if (m_PollingActive == active) {
        return;
    }

    m_PollingActive = active;
    emit pollingActiveChanged();
}
