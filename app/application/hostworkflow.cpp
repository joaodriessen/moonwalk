#include "hostworkflow.h"

HostWorkflow::HostWorkflow(QObject* parent)
    : QObject(parent)
{
}

int HostWorkflow::currentPairingAttemptId() const
{
    return m_CurrentPairingAttemptId;
}

void HostWorkflow::setCurrentPairingAttemptId(int attemptId)
{
    if (m_CurrentPairingAttemptId == attemptId) {
        return;
    }
    m_CurrentPairingAttemptId = attemptId;
    emit stateChanged();
}

QVariantList HostWorkflow::pendingPairingAttempts() const
{
    return m_PendingPairingAttempts;
}

void HostWorkflow::setPendingPairingAttempts(const QVariantList& attempts)
{
    if (m_PendingPairingAttempts == attempts) {
        return;
    }
    m_PendingPairingAttempts = attempts;
    emit stateChanged();
}

QVariantMap HostWorkflow::canceledPairingAttempts() const
{
    return m_CanceledPairingAttempts;
}

void HostWorkflow::setCanceledPairingAttempts(const QVariantMap& attempts)
{
    if (m_CanceledPairingAttempts == attempts) {
        return;
    }
    m_CanceledPairingAttempts = attempts;
    emit stateChanged();
}

int HostWorkflow::beginPairingAttempt(int attemptId)
{
    if (attemptId < 0 || m_PendingPairingAttempts.contains(attemptId)) {
        return -1;
    }
    m_PendingPairingAttempts.append(attemptId);
    m_CurrentPairingAttemptId = attemptId;
    emit stateChanged();
    return attemptId;
}

void HostWorkflow::cancelCurrentPairingAttempt()
{
    if (m_CurrentPairingAttemptId < 0) {
        return;
    }

    m_CanceledPairingAttempts.insert(QString::number(m_CurrentPairingAttemptId), true);
    emit stateChanged();
}

QVariantMap HostWorkflow::completePairingAttempt(int attemptId, const QVariant& error)
{
    QVariantMap completion {
        { QStringLiteral("attemptId"), -1 },
        { QStringLiteral("closeCurrent"), false },
        { QStringLiteral("showError"), false },
        { QStringLiteral("error"), error },
    };

    if (!m_PendingPairingAttempts.removeOne(attemptId)) {
        return completion;
    }

    const QString attemptKey = QString::number(attemptId);
    const bool wasCanceled = m_CanceledPairingAttempts.take(attemptKey).toBool();
    const bool isCurrent = attemptId == m_CurrentPairingAttemptId;

    completion.insert(QStringLiteral("attemptId"), attemptId);
    if (isCurrent) {
        completion.insert(QStringLiteral("closeCurrent"), true);
        completion.insert(QStringLiteral("showError"), error.isValid() && !wasCanceled);
        m_CurrentPairingAttemptId = -1;
    }

    emit stateChanged();
    return completion;
}
