import QtQuick 2.15
import QtTest 1.3
import HostListModel 1.0

TestCase {
    id: testCase
    name: "HostListModelImport"

    Component {
        id: proxyComponent
        HostListModel {
        }
    }

    function test_proxy_is_creatable_and_exposes_no_backend_methods() {
        const proxy = proxyComponent.createObject(null)
        verify(proxy !== undefined && proxy !== null)
        if (proxy === undefined || proxy === null) {
            return
        }
        ["pair", "wake", "deleteComputer", "createSessionForDesktop",
         "quitRunningApp", "setSourceModel", "setSourceModelForCpp"].forEach(
                    function(name) {
            verify(typeof proxy[name] !== "function",
                   name + " must not reach QML")
        })
        compare(proxy.sourceModel, null)
        proxy.destroy()
    }
}
