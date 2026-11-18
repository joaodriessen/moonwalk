import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Window 2.15
import "../../starlight" as AppStyle

T.Menu {
    id: root

    property string accessibleName: title
    property string accessibleDescription: ""
    readonly property Item accessibleItem: semanticItem
    readonly property GnomeTokens tokens: GnomeTokens {}

    padding: 8
    // Stay inside the window proper, not just its transparent shadow margin.
    margins: 6 + (typeof window !== "undefined" && window.csdMargin !== undefined ? window.csdMargin : 0)
    overlap: 1
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    font.pixelSize: tokens.bodySize
    delegate: GnomeMenuItem {}
    contentItem: ListView {
        implicitHeight: contentHeight
        model: root.contentModel
        interactive: Window.window
                     ? contentHeight + root.topPadding + root.bottomPadding > root.height
                     : false
        clip: true
        currentIndex: root.currentIndex
    }

    // Size to the widest item so labels never truncate. The Menu's own
    // implicitContentWidth is ListView-backed and reports too small, so
    // measure the items directly.
    function _recomputeWidth() {
        var widest = 0
        for (var i = 0; i < count; ++i) {
            var item = itemAt(i)
            if (item)
                widest = Math.max(widest, item.implicitWidth)
        }
        contentWidth = widest
    }
    onCountChanged: Qt.callLater(_recomputeWidth)
    onAboutToShow: _recomputeWidth()
    Component.onCompleted: Qt.callLater(_recomputeWidth)

    // Use the same motion policy as the other GNOME popovers/dialogs rather
    // than inheriting Material's fixed-duration grow/shrink animations.
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: root.tokens.reducedMotion ? 0 : 120; easing.type: Easing.InCubic }
    }

    // popover.background > contents:
    //   border: 1px solid RGB(0 0 0 / 14%);
    //   box-shadow: 0 1px 5px 1px RGB(0 0 0 / 9%),
    //               0 2px 14px 3px RGB(0 0 0 / 5%);
    // One border colour for both schemes -- the stylesheet defines it once.
    background: Rectangle {
        implicitWidth: 120
        radius: root.tokens.radiusPopup
        color: root.tokens.popover
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.14)

        // Both shadow layers are offset downward, so this reaches 16px below
        // the popover but only 12px above it. Margins and borders come from
        // the native GTK render (scripts/design/gnome-shadows.py).
        BorderImage {
            objectName: "popover.shadow"
            anchors.fill: parent
            anchors.leftMargin: -14
            anchors.rightMargin: -14
            anchors.topMargin: -12
            anchors.bottomMargin: -16
            source: "qrc:/res/popover_shadow.png"
            border { left: 36; right: 36; top: 36; bottom: 36 }
            z: -1
        }
    }

    property Item _semanticItem: Item {
        id: semanticItem
        parent: root.contentItem
        Accessible.role: Accessible.PopupMenu
        Accessible.name: root.accessibleName
        Accessible.description: root.accessibleDescription

        AppStyle.AccessibleIdentity {
            target: semanticItem
            stableId: root.objectName
        }
    }
}
