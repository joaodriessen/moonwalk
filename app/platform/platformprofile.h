#pragma once

#include <QObject>
#include <QStringList>

class PlatformProfile final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Kind kind READ kind CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString selectionReason READ selectionReason CONSTANT)
    Q_PROPERTY(QStringList confirmedCapabilities READ confirmedCapabilities CONSTANT)

public:
    enum Kind {
        Generic,
        Gnome,
        Plasma,
    };
    Q_ENUM(Kind)

    PlatformProfile(Kind kind, QString selectionReason, QStringList confirmedCapabilities, QObject* parent = nullptr);

    Kind kind() const;
    QString name() const;
    QString selectionReason() const;
    QStringList confirmedCapabilities() const;

private:
    const Kind m_Kind;
    const QString m_SelectionReason;
    const QStringList m_ConfirmedCapabilities;
};

Q_DECLARE_METATYPE(PlatformProfile::Kind)
