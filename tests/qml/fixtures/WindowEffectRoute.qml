import QtQuick 2.15
import QtQuick.Controls 2.15

import "../../../app/gui/adaptive" as Adaptive

// Test-only route boundary: it uses the real HostPageContext but deliberately
// records effects instead of forwarding them. Tests can therefore exercise
// main.qml's real public consumer without any mutable production override.
Item {
    id: root

    readonly property string shellRouteKind: "hosts"
    readonly property bool shellRouteActive: StackView.status === StackView.Active
    property var hostSource: null
    property var lastEffect: null
    readonly property var context: contextObject

    Adaptive.HostPageContext {
        id: contextObject
        hostSource: root.hostSource
        routeActive: root.shellRouteActive
    }

    Connections {
        target: contextObject
        function onWindowEffectRequested(effect) {
            root.lastEffect = effect
        }
    }
}
