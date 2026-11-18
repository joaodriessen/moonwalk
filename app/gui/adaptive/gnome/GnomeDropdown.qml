import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Window 2.15
import "../../starlight" as AppStyle
import "../../starlight/Lucide.js" as Lucide

T.ComboBox {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: ""
    property string accessibleDescription: ""
    property bool flatStyle: false
    readonly property GnomeTokens tokens: GnomeTokens {}

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    leftPadding: tokens.spacingM + (mirrored ? indicator.width + tokens.spacingS : 0)
    rightPadding: tokens.spacingM + (mirrored ? 0 : indicator.width + tokens.spacingS)
    topPadding: tokens.spacingS
    bottomPadding: tokens.spacingS
    implicitWidth: Math.max(140,
                            contentItem.implicitWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(tokens.fieldHeight,
                             contentItem.implicitHeight
                             + topPadding + bottomPadding)
    font.pixelSize: tokens.bodySize

    function controllerActivate() {
        if (!root.enabled) {
            return
        }
        if (root.popup.opened) {
            root.popup.close()
        } else {
            root.popup.open()
        }
    }

    contentItem: Text {
        text: root.displayText
        font: root.font
        color: root.enabled
               ? (root.flatStyle
                  ? root.tokens.secondaryText : root.tokens.text)
               : root.tokens.disabledText
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: root.flatStyle
                             ? (root.mirrored
                                ? Text.AlignLeft : Text.AlignRight)
                             : (root.mirrored
                                ? Text.AlignRight : Text.AlignLeft)
        elide: Text.ElideRight
    }

    // Adwaita pan-down-symbolic, drawn as a chevron from two strokes.
    indicator: Item {
        width: 16
        height: 16
        x: root.mirrored
           ? root.tokens.spacingM
           : root.width - width - root.tokens.spacingM
        y: Math.round((root.height - height) / 2)

        readonly property color ink: root.enabled
            ? root.tokens.secondaryText : root.tokens.disabledText
        readonly property real stroke: 1.5

        Rectangle {
            x: parent.width / 2 - width + parent.stroke / 2
            y: parent.height * 0.44
            width: Math.round(parent.width * 0.3)
            height: parent.stroke
            radius: height / 2
            color: parent.ink
            rotation: 45
            transformOrigin: Item.Right
        }
        Rectangle {
            x: parent.width / 2 - parent.stroke / 2
            y: parent.height * 0.44
            width: Math.round(parent.width * 0.3)
            height: parent.stroke
            radius: height / 2
            color: parent.ink
            rotation: -45
            transformOrigin: Item.Left
        }
    }

    background: Rectangle {
        implicitHeight: root.tokens.fieldHeight
        radius: root.tokens.radiusControl
        color: {
            if (root.flatStyle && !root.down && !root.hovered)
                return "transparent"
            return root.down
                   ? root.tokens.controlPressed
                   : (root.hovered
                      ? root.tokens.controlHover
                      : root.tokens.control)
        }
        border.width: root.visualFocus
                      ? root.tokens.focusWidth
                      : (root.tokens.highContrast && !root.flatStyle ? 2 : 0)
        border.color: root.visualFocus
                      ? root.tokens.focus
                      : root.tokens.border
    }

    delegate: T.ItemDelegate {
        id: option
        required property var model
        required property int index

        width: ListView.view ? ListView.view.width : root.width
        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        implicitHeight: Math.max(32, implicitContentHeight + topPadding + bottomPadding)
        hoverEnabled: true
        text: root.textRole ? model[root.textRole] : model.modelData
        highlighted: root.highlightedIndex === index
        font.pixelSize: root.tokens.bodySize
        leftPadding: root.tokens.spacingM
        rightPadding: root.tokens.spacingM
        topPadding: 6
        bottomPadding: 6

        contentItem: Row {
            spacing: root.tokens.spacingS
            Text {
                width: option.availableWidth - check.width - parent.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: option.text
                font: option.font
                color: root.tokens.text
                wrapMode: Text.WordWrap
            }
            Image {
                id: check
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                sourceSize: Qt.size(32, 32)
                opacity: root.currentIndex === option.index ? 1 : 0
                source: Lucide.url("check", root.tokens.dark ? "#ffffff" : "#222226")
            }
        }
        // Adwaita menu selection is a neutral 10% fill, never a loud
        // solid-accent bar.
        background: Rectangle {
            radius: root.tokens.radiusControl
            color: option.highlighted || option.hovered
                   ? (root.tokens.dark ? Qt.rgba(1, 1, 1, 0.10)
                                       : Qt.rgba(0, 0, 6 / 255, 0.10))
                   : "transparent"
        }
    }

    popup: T.Popup {
        id: dropdownPopup
        // Wide enough for the options even when the row keeps its value short.
        property real optionsWidth: 0
        function measureOptions() {
            var widest = root.width
            metrics.font = root.font
            for (var i = 0; i < root.count; ++i) {
                metrics.text = root.textAt(i)
                widest = Math.max(widest, metrics.advanceWidth + 2 * root.tokens.spacingM
                                  + 16 + 2 * root.tokens.spacingS + 4)
            }
            optionsWidth = Math.ceil(widest)
        }
        onAboutToShow: measureOptions()
        TextMetrics { id: metrics }
        x: root.mirrored ? 0 : root.width - width
        y: root.height + root.tokens.spacingXS
        width: Math.max(root.width, Math.min(optionsWidth, root.Window.window ? root.Window.window.width - 32 : optionsWidth))
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 340)
        margins: 8
        padding: root.tokens.spacingXS + 2
        modal: false
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.tokens.motionDuration; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: root.tokens.reducedMotion ? 0 : 100; easing.type: Easing.InCubic }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: GnomeScrollBar {}
        }

        background: Rectangle {
            radius: root.tokens.radiusPopup
            color: root.tokens.popover
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.14)
            BorderImage {
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
    }

    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleName.length > 0
                     ? accessibleName : displayText
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.onPressAction: controllerActivate()

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
