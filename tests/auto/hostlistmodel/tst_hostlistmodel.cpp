#include <QtTest>

#include <algorithm>
#include <memory>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardItemModel>

#include "gui/hostidentitysource.h"
#include "gui/hostlistmodel.h"

class FakeHostModel final : public QAbstractListModel,
                            public HostIdentitySource
{
public:
    enum Roles {
        NameRole = Qt::UserRole + 1
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_Hosts.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_Hosts.size()
                || role != NameRole) {
            return QVariant();
        }
        return m_Hosts.at(index.row()).second;
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return { { NameRole, "name" } };
    }

    QString uuidForRow(int row) const override
    {
        return row >= 0 && row < m_Hosts.size() ? m_Hosts.at(row).first : QString();
    }

    void replaceHosts(const QList<QPair<QString, QString>>& hosts)
    {
        beginResetModel();
        m_Hosts = hosts;
        endResetModel();
    }

    void appendHost(const QString& uuid, const QString& name)
    {
        const int row = m_Hosts.size();
        beginInsertRows(QModelIndex(), row, row);
        m_Hosts.append({ uuid, name });
        endInsertRows();
    }

    void removeLastHost()
    {
        const int row = m_Hosts.size() - 1;
        beginRemoveRows(QModelIndex(), row, row);
        m_Hosts.removeAt(row);
        endRemoveRows();
    }

    void moveFirstHostToEnd()
    {
        beginMoveRows(QModelIndex(), 0, 0, QModelIndex(), m_Hosts.size());
        m_Hosts.move(0, m_Hosts.size() - 1);
        endMoveRows();
    }

    void renameHost(int row, const QString& name, bool allRoles = false)
    {
        m_Hosts[row].second = name;
        emit dataChanged(index(row, 0), index(row, 0), allRoles ? QList<int>() : QList<int> { NameRole });
    }

    void reverseHosts()
    {
        emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
        std::reverse(m_Hosts.begin(), m_Hosts.end());
        emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
    }

private:
    QList<QPair<QString, QString>> m_Hosts;
};

class HostListModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void forwardsResetAndRoles();
    void sourceDestructionCannotExposeADestroyedModelDuringReset();
    void forwardsAllowlistedChanges();
    void allRolesChangeRefreshesQmlDelegate();
    void requiresIdentityAndRefusesReplacement();
    void qmlConstructionAssignmentIsOpaqueAndInitializeOnce();
    void exportsNoMutationMethods();
};

void HostListModelTest::forwardsResetAndRoles()
{
    FakeHostModel source;
    HostListModel proxy;
    QVERIFY(proxy.setSourceModelForCpp(&source));
    QSignalSpy reset(&proxy, &QAbstractItemModel::modelReset);

    source.replaceHosts({ { QStringLiteral("a"), QStringLiteral("Alpha") },
                          { QStringLiteral("b"), QStringLiteral("Beta") } });

    QCOMPARE(reset.count(), 1);
    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.data(proxy.index(1, 0), FakeHostModel::NameRole).toString(),
             QStringLiteral("Beta"));
    QCOMPARE(proxy.data(proxy.index(1, 0), HostListModel::UuidRole).toString(),
             QStringLiteral("b"));
    QVERIFY(!source.roleNames().values().contains(QByteArrayLiteral("uuid")));
    QCOMPARE(proxy.roleNames().value(HostListModel::UuidRole),
             QByteArrayLiteral("uuid"));
}

void HostListModelTest::forwardsAllowlistedChanges()
{
    FakeHostModel source;
    HostListModel proxy;
    QVERIFY(proxy.setSourceModelForCpp(&source));
    source.replaceHosts({ { QStringLiteral("a"), QStringLiteral("Alpha") },
                          { QStringLiteral("b"), QStringLiteral("Beta") } });

    QSignalSpy dataChanged(&proxy, &QAbstractItemModel::dataChanged);
    QSignalSpy inserted(&proxy, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&proxy, &QAbstractItemModel::rowsRemoved);
    QSignalSpy moved(&proxy, &QAbstractItemModel::rowsMoved);
    QSignalSpy layoutChanged(&proxy, &QAbstractItemModel::layoutChanged);

    source.renameHost(1, QStringLiteral("Bravo"));
    QCOMPARE(dataChanged.count(), 1);
    const auto explicitRoles = qvariant_cast<QList<int>>(dataChanged.first().at(2));
    QCOMPARE(explicitRoles, (QList<int> { FakeHostModel::NameRole, HostListModel::UuidRole }));
    QCOMPARE(proxy.data(proxy.index(1, 0), FakeHostModel::NameRole).toString(),
             QStringLiteral("Bravo"));

    source.appendHost(QStringLiteral("c"), QStringLiteral("Charlie"));
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(proxy.rowCount(), 3);

    source.moveFirstHostToEnd();
    QCOMPARE(moved.count(), 1);
    QCOMPARE(proxy.data(proxy.index(2, 0), HostListModel::UuidRole).toString(),
             QStringLiteral("a"));

    source.reverseHosts();
    QCOMPARE(layoutChanged.count(), 1);

    source.removeLastHost();
    QCOMPARE(removed.count(), 1);
    QCOMPARE(proxy.rowCount(), 2);
}

void HostListModelTest::allRolesChangeRefreshesQmlDelegate()
{
    FakeHostModel source;
    HostListModel proxy;
    QVERIFY(proxy.setSourceModelForCpp(&source));
    source.replaceHosts({ { QStringLiteral("a"), QStringLiteral("Before poll") } });
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hostProxy"), &proxy);
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml 2.15
        import QtQml.Models 2.15
        Instantiator {
            model: hostProxy
            delegate: QtObject { required property string name }
        }
    )", QUrl());
    std::unique_ptr<QObject> view(component.create());
    QVERIFY2(view, qPrintable(component.errorString()));
    QObject* delegate = nullptr;
    QVERIFY(QMetaObject::invokeMethod(view.get(), "objectAt", Q_RETURN_ARG(QObject*, delegate), Q_ARG(int, 0)));
    QVERIFY(delegate);
    QCOMPARE(delegate->property("name").toString(), QStringLiteral("Before poll"));
    QSignalSpy changed(&proxy, &QAbstractItemModel::dataChanged);

    // ComputerModel's real polling updates use an empty role list (all roles).
    source.renameHost(0, QStringLiteral("After poll"), true);
    QTRY_COMPARE(delegate->property("name").toString(), QStringLiteral("After poll"));
    QCOMPARE(changed.count(), 1);
    QVERIFY(qvariant_cast<QList<int>>(changed.first().at(2)).isEmpty());
}

void HostListModelTest::requiresIdentityAndRefusesReplacement()
{
    QStandardItemModel plainSource;
    FakeHostModel firstSource;
    FakeHostModel replacementSource;
    HostListModel proxy;

    QVERIFY(!proxy.setSourceModelForCpp(&plainSource));
    QVERIFY(proxy.setSourceModelForCpp(&firstSource));
    QVERIFY(!proxy.setSourceModelForCpp(&replacementSource));

    firstSource.replaceHosts({ { QStringLiteral("first"), QStringLiteral("First") } });
    replacementSource.replaceHosts({ { QStringLiteral("replacement"), QStringLiteral("Replacement") } });
    QCOMPARE(proxy.data(proxy.index(0, 0), HostListModel::UuidRole).toString(),
             QStringLiteral("first"));
}

void HostListModelTest::qmlConstructionAssignmentIsOpaqueAndInitializeOnce()
{
    qmlRegisterType<HostListModel>("HostListModel", 1, 0, "HostListModel");

    FakeHostModel firstSource;
    FakeHostModel replacementSource;
    firstSource.replaceHosts({ { QStringLiteral("first"), QStringLiteral("First") } });
    replacementSource.replaceHosts({ { QStringLiteral("replacement"), QStringLiteral("Replacement") } });

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("firstSource"), &firstSource);
    engine.rootContext()->setContextProperty(QStringLiteral("replacementSource"), &replacementSource);
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml 2.15
        import HostListModel 1.0
        HostListModel {
            sourceModel: firstSource
            property var observedSource
            Component.onCompleted: {
                observedSource = sourceModel
                sourceModel = replacementSource
            }
        }
    )", QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QTest::ignoreMessage(QtWarningMsg,
                         "HostListModel sourceModel must be assigned once before component completion");
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));

    auto* proxy = qobject_cast<HostListModel*>(object.get());
    QVERIFY(proxy != nullptr);
    QCOMPARE(proxy->rowCount(), 1);
    QCOMPARE(proxy->data(proxy->index(0, 0), HostListModel::UuidRole).toString(),
             QStringLiteral("first"));
    QVERIFY(object->property("observedSource").isNull());
    QVERIFY(!proxy->setSourceModelForCpp(&replacementSource));
}

void HostListModelTest::exportsNoMutationMethods()
{
    HostListModel proxy;
    const QMetaObject* meta = proxy.metaObject();
    const QSet<QByteArray> forbidden = {
        QByteArrayLiteral("pair"),
        QByteArrayLiteral("wake"),
        QByteArrayLiteral("deleteComputer"),
        QByteArrayLiteral("createSessionForDesktop"),
        QByteArrayLiteral("quitRunningApp"),
        QByteArrayLiteral("setSourceModelForCpp"),
        QByteArrayLiteral("setSourceModel")
    };

    for (int index = 0; index < meta->methodCount(); ++index) {
        QVERIFY(!forbidden.contains(meta->method(index).name()));
    }

    const int sourcePropertyIndex = meta->indexOfProperty("sourceModel");
    QVERIFY(sourcePropertyIndex >= 0);
    const QMetaProperty sourceProperty = meta->property(sourcePropertyIndex);
    QVERIFY(sourceProperty.isReadable());
    QVERIFY(sourceProperty.isWritable());
    QVERIFY(!sourceProperty.hasNotifySignal());
    QVERIFY(sourceProperty.read(&proxy).isNull());
}

QTEST_MAIN(HostListModelTest)

#include "tst_hostlistmodel.moc"

void HostListModelTest::sourceDestructionCannotExposeADestroyedModelDuringReset()
{
    HostListModel proxy;
    auto source=std::make_unique<FakeHostModel>();
    source->replaceHosts({{QStringLiteral("a"),QStringLiteral("Alpha")}});
    QVERIFY(proxy.setSourceModelForCpp(source.get()));
    bool observed=false;
    connect(&proxy,&QAbstractItemModel::modelAboutToBeReset,&proxy,[&] {
        observed=true;
        // QML's delegate model performs this query while handling reset.
        QVERIFY(proxy.roleNames().contains(HostListModel::UuidRole));
        QCOMPARE(proxy.rowCount(),0);
    });
    source.reset();
    QVERIFY(observed);
    QCOMPARE(proxy.rowCount(),0);
}
