#pragma once

#include <QObject>
#include <QVariantMap>

class PollingLifecycle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool pollingActive READ pollingActive NOTIFY pollingActiveChanged)

public:
    explicit PollingLifecycle(QObject* parent = nullptr);

    bool pollingActive() const;

    Q_INVOKABLE QVariantMap visibilityChanged(bool visible, bool active);
    Q_INVOKABLE QVariantMap activityChanged(bool active, bool visible);
    Q_INVOKABLE QVariantMap inactivityExpired(bool active);
    Q_INVOKABLE void commitPollingState(bool active);

signals:
    void pollingActiveChanged();

private:
    QVariantMap decision(bool hasFocus) const;

    bool m_PollingActive = false;
};
