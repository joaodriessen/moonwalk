#include "hostlistmodel.h"

#include "hostidentitysource.h"

#include <QDebug>

HostListModel::HostListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

HostListModel::~HostListModel()
{
    disconnectSource(false);
}

void HostListModel::classBegin()
{
    m_ComponentComplete = false;
}

void HostListModel::componentComplete()
{
    m_ComponentComplete = true;
}

bool HostListModel::setSourceModelForCpp(QAbstractItemModel* sourceModel)
{
    return attachSource(sourceModel);
}

void HostListModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    if (!attachSource(sourceModel)) {
        qWarning() << "HostListModel sourceModel must be assigned once before component completion";
    }
}

int HostListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_SourceModel) {
        return 0;
    }
    return m_SourceModel->rowCount();
}

QVariant HostListModel::data(const QModelIndex& index, int role) const
{
    if (!m_SourceModel || !index.isValid() || index.parent().isValid()
            || index.row() < 0 || index.row() >= m_SourceModel->rowCount()) {
        return QVariant();
    }

    if (role == UuidRole) {
        return m_IdentitySource ? m_IdentitySource->uuidForRow(index.row())
                                : QString();
    }

    return m_SourceModel->data(m_SourceModel->index(index.row(), index.column()), role);
}

QHash<int, QByteArray> HostListModel::roleNames() const
{
    QHash<int, QByteArray> roles = m_SourceModel ? m_SourceModel->roleNames()
                                                  : QHash<int, QByteArray>();
    for (auto it = roles.begin(); it != roles.end();) {
        if (it.value() == QByteArrayLiteral("uuid")) {
            it = roles.erase(it);
        }
        else {
            ++it;
        }
    }
    roles.insert(UuidRole, QByteArrayLiteral("uuid"));
    return roles;
}

bool HostListModel::attachSource(QAbstractItemModel* sourceModel)
{
    if (m_ComponentComplete || m_SourceAssigned || !sourceModel) {
        return false;
    }

    auto* identitySource = dynamic_cast<HostIdentitySource*>(sourceModel);
    if (!identitySource) {
        return false;
    }

    m_SourceAssigned = true;
    m_SourceModel = sourceModel;
    m_IdentitySource = identitySource;

    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::dataChanged,
        this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight,
                     const QList<int>& roles) {
            if (topLeft.parent().isValid() || bottomRight.parent().isValid()) {
                return;
            }
            QList<int> forwardedRoles = roles;
            // Empty means every role changed; narrowing it to UUID would leave
            // QML delegates showing their old online/pairing/status values.
            if (!forwardedRoles.isEmpty() && !forwardedRoles.contains(UuidRole)) {
                forwardedRoles.append(UuidRole);
            }
            emit dataChanged(index(topLeft.row(), topLeft.column()),
                             index(bottomRight.row(), bottomRight.column()),
                             forwardedRoles);
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeInserted,
        this, [this](const QModelIndex& parent, int first, int last) {
            if (!parent.isValid()) {
                beginInsertRows(QModelIndex(), first, last);
            }
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsInserted,
        this, [this](const QModelIndex& parent, int, int) {
            if (!parent.isValid()) {
                endInsertRows();
            }
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, [this](const QModelIndex& parent, int first, int last) {
            if (!parent.isValid()) {
                beginRemoveRows(QModelIndex(), first, last);
            }
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsRemoved,
        this, [this](const QModelIndex& parent, int, int) {
            if (!parent.isValid()) {
                endRemoveRows();
            }
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeMoved,
        this, [this](const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
                     const QModelIndex& destinationParent, int destinationRow) {
            if (!sourceParent.isValid() && !destinationParent.isValid()) {
                beginMoveRows(QModelIndex(), sourceStart, sourceEnd,
                              QModelIndex(), destinationRow);
            }
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsMoved,
        this, [this](const QModelIndex& sourceParent, int, int,
                     const QModelIndex& destinationParent, int) {
            if (!sourceParent.isValid() && !destinationParent.isValid()) {
                endMoveRows();
            }
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::layoutAboutToBeChanged,
        this, [this](const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint hint) {
            emit layoutAboutToBeChanged({}, hint);
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::layoutChanged,
        this, [this](const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint hint) {
            emit layoutChanged({}, hint);
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset,
        this, [this]() {
            beginResetModel();
        }));
    m_SourceConnections.append(connect(sourceModel, &QAbstractItemModel::modelReset,
        this, [this]() {
            endResetModel();
        }));
    m_SourceConnections.append(connect(sourceModel, &QObject::destroyed,
        this, [this]() {
            disconnectSource(true);
        }));
    return true;
}

void HostListModel::disconnectSource(bool notifyReset)
{
    if (!m_SourceModel) {
        return;
    }

    // QObject::destroyed runs after the source's derived model has gone away.
    // Reset listeners may synchronously ask for roles or rows; detach first so
    // those queries never call virtual methods on the dying source.
    m_SourceModel = nullptr;
    m_IdentitySource = nullptr;
    if (notifyReset) {
        beginResetModel();
    }
    for (const QMetaObject::Connection& connection : std::as_const(m_SourceConnections)) {
        disconnect(connection);
    }
    m_SourceConnections.clear();
    if (notifyReset) {
        endResetModel();
    }
}
