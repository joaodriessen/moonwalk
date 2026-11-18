#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class HostWorkflow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentPairingAttemptId READ currentPairingAttemptId
               WRITE setCurrentPairingAttemptId NOTIFY stateChanged)
    Q_PROPERTY(QVariantList pendingPairingAttempts READ pendingPairingAttempts
               WRITE setPendingPairingAttempts NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap canceledPairingAttempts READ canceledPairingAttempts
               WRITE setCanceledPairingAttempts NOTIFY stateChanged)

public:
    explicit HostWorkflow(QObject* parent = nullptr);

    int currentPairingAttemptId() const;
    void setCurrentPairingAttemptId(int attemptId);

    QVariantList pendingPairingAttempts() const;
    void setPendingPairingAttempts(const QVariantList& attempts);

    QVariantMap canceledPairingAttempts() const;
    void setCanceledPairingAttempts(const QVariantMap& attempts);

    Q_INVOKABLE int beginPairingAttempt(int attemptId);
    Q_INVOKABLE void cancelCurrentPairingAttempt();
    Q_INVOKABLE QVariantMap completePairingAttempt(int attemptId, const QVariant& error = QVariant());

signals:
    void stateChanged();

private:
    int m_CurrentPairingAttemptId = -1;
    QVariantList m_PendingPairingAttempts;
    QVariantMap m_CanceledPairingAttempts;
};
