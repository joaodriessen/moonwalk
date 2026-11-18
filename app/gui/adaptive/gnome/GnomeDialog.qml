import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Window 2.15
import "../../starlight" as AppStyle

T.Dialog {
    id: root

    property string accessibleName: title
    property string accessibleDescription: ""
    // Qt exposes the popup item's Dialog role to AT-SPI. Put the body
    // description there rather than adding a second semantic dialog child.
    readonly property Item accessibleItem: contentItem ? contentItem.parent : null
    readonly property GnomeTokens tokens: GnomeTokens {}

    // AdwDialog keeps Close in its header bar; alerts answer with responses instead.
    property bool closeInHeader: false
    property alias headerCloseButton: headerClose
    property string closeObjectName: "gnome-dialog-close"
    signal closeRequested()
    // AdwDialog becomes a bottom sheet when the window is too small to float in.
    readonly property bool sheet: !!parent && parent.width < 460
    readonly property int outerMargin: parent && parent.Window.window && parent.Window.window.csdMargin !== undefined
                                       ? parent.Window.window.csdMargin : 0

    // Declared inside a pane that may be hidden; it belongs to the whole window.
    parent: Overlay.overlay
    modal: true
    anchors.centerIn: sheet ? null : parent
    x: sheet ? outerMargin : 0
    // From the implicit height: reading the height a Popup is settling would loop.
    y: sheet ? parent.height - fittedHeight - outerMargin : 0
    // As tall as it wants to be, up to what the window has room for.
    readonly property real fittedHeight: parent ? Math.min(implicitHeight, parent.height - 2 * outerMargin - 2 * tokens.spacingM)
                                                : implicitHeight
    function fittedWidth(preferred) {
        return !parent ? preferred : sheet ? parent.width - 2 * outerMargin
                                           : Math.min(preferred, parent.width - 2 * tokens.spacingM)
    }
    padding: tokens.spacingL
    topPadding: tokens.spacingM
    implicitWidth: Math.max(340,
                            implicitContentWidth
                            + leftPadding + rightPadding,
                            implicitHeaderWidth, implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    footer: DialogButtonBox {
        visible: count > 0
    }
    header: Item {
        visible: root.title.length > 0
        implicitHeight: root.closeInHeader ? root.tokens.toolbarHeight : headerLabel.implicitHeight
        implicitWidth: headerLabel.implicitWidth
        Label {
            id: headerLabel
            anchors.fill: parent
            anchors.leftMargin: root.closeInHeader ? 48 : 0
            anchors.rightMargin: root.closeInHeader ? 48 : 0
            text: root.title
            color: root.tokens.text
            font.bold: true
            font.pixelSize: root.closeInHeader ? root.tokens.bodySize : root.tokens.titleSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            leftPadding: root.closeInHeader ? 0 : root.tokens.spacingL
            rightPadding: leftPadding
            topPadding: root.closeInHeader ? 0 : root.tokens.spacingL
            bottomPadding: root.closeInHeader ? 0 : root.tokens.spacingXS
            elide: Text.ElideRight
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
        GnomeHeaderButton {
            id: headerClose
            objectName: root.closeObjectName
            visible: root.closeInHeader
            anchors.right: parent.right
            anchors.rightMargin: root.tokens.headerbarPaddingRight
            anchors.verticalCenter: parent.verticalCenter
            symbol: "close"
            accessibleName: qsTranslate("PcView", "Close")
            onClicked: root.closeRequested()
            Keys.onReturnPressed: root.closeRequested()
            Keys.onEnterPressed: root.closeRequested()
        }
    }

    background: Rectangle {
        // A sheet meets the window's bottom edge, so only its top corners round.
        radius: root.tokens.radiusDialog
        Rectangle {
            // A rounded window keeps the sheet's own rounding at its corners.
            visible: root.sheet && root.outerMargin === 0
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: parent.radius
            color: parent.color
        }
        color: root.tokens.dialogSurface
        border.width: root.tokens.highContrast
                      ? root.tokens.borderWidth
                      : (root.tokens.dark ? 1 : 0)
        border.color: root.tokens.dark
                      ? Qt.rgba(1, 1, 1, 0.1) : root.tokens.border
    }

    dim: true

    // Dim stays inside the CSD margin and follows the window radius, so a
    // dialog never squares off the rounded window corners.
    Overlay.modal: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: Window.window && Window.window.csdMargin !== undefined
                             ? Window.window.csdMargin : 0
            radius: Window.window && Window.window.csdRadius !== undefined
                    ? Window.window.csdRadius : 0
            color: Qt.rgba(0, 0, 0, 0.32)
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: root.sheet ? 1.0 : 0.96; to: 1.0; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: root.tokens.reducedMotion ? 0 : 120; easing.type: Easing.InCubic }
    }

    function synchronizeAccessibility() {
        if (!accessibleItem) return
        accessibleItem.Accessible.role = Accessible.Dialog
        accessibleItem.Accessible.name = accessibleName
        accessibleItem.Accessible.description = accessibleDescription
    }
    onAccessibleItemChanged: synchronizeAccessibility()
    onAccessibleNameChanged: synchronizeAccessibility()
    onAccessibleDescriptionChanged: synchronizeAccessibility()
    Component.onCompleted: synchronizeAccessibility()

    property QtObject _accessibleIdentity: AppStyle.AccessibleIdentity {
        target: root.accessibleItem
        stableId: root.objectName
    }
}
