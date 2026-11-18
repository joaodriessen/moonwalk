#pragma once

#include <QAbstractListModel>
#include <QMetaObject>
#include <QQmlParserStatus>

class HostIdentitySource;

class HostListModel : public QAbstractListModel,
                      public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    // Qt requires a READ or MEMBER backing for every property. This sentinel
    // is intentionally never assigned: QML can write sourceModel during
    // construction, but reads can never recover the private backend.
    Q_PROPERTY(QAbstractItemModel* sourceModel MEMBER m_SourceModelSentinel WRITE setSourceModel)

public:
    enum Roles {
        UuidRole = Qt::UserRole + 1024
    };

    explicit HostListModel(QObject* parent = nullptr);
    ~HostListModel() override;

    void classBegin() override;
    void componentComplete() override;

    bool setSourceModelForCpp(QAbstractItemModel* sourceModel);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSourceModel(QAbstractItemModel* sourceModel);

private:
    bool attachSource(QAbstractItemModel* sourceModel);
    void disconnectSource(bool notifyReset);

    QAbstractItemModel* m_SourceModel = nullptr;
    QAbstractItemModel* m_SourceModelSentinel = nullptr;
    HostIdentitySource* m_IdentitySource = nullptr;
    QList<QMetaObject::Connection> m_SourceConnections;
    bool m_ComponentComplete = false;
    bool m_SourceAssigned = false;
};
