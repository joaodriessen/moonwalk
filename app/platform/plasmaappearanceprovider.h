#pragma once

#include <QObject>
#include <QPalette>
#include <QString>
#include <QVariantMap>

class PlasmaAppearanceProvider final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY snapshotChanged)
    Q_PROPERTY(quint64 revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantMap active READ active NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantMap inactive READ inactive NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantMap disabled READ disabled NOTIFY snapshotChanged)
    Q_PROPERTY(QString widgetStyle READ widgetStyle NOTIFY snapshotChanged)

public:
    explicit PlasmaAppearanceProvider(
            bool runtimeActive,
            QObject* parent = nullptr);

    static bool runtimeActiveForAdapter(
            bool plasmaAdapterActive,
            bool adaptiveShellRequested);

    bool ready() const;
    quint64 revision() const;
    QVariantMap active() const;
    QVariantMap inactive() const;
    QVariantMap disabled() const;
    QString widgetStyle() const;

    void applySnapshotForTest(
            const QPalette& palette,
            const QString& widgetStyle);

public slots:
    void requestRefresh();
    void handleKdeGlobalSettingsChange(int changeType, int argument);

signals:
    void snapshotChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    static QVariantMap colorGroup(
            const QPalette& palette,
            QPalette::ColorGroup group);
    void refreshNow();
    void publishSnapshot(
            const QPalette& palette,
            const QString& widgetStyle);

    bool m_Ready = false;
    bool m_RefreshPending = false;
    quint64 m_Revision = 0;
    QVariantMap m_Active;
    QVariantMap m_Inactive;
    QVariantMap m_Disabled;
    QString m_WidgetStyle;
};
