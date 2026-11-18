import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle

T.Slider {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: ""
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}

    focusPolicy: Qt.TabFocus
    implicitWidth: 220
    implicitHeight: tokens.controlHeight
    // Production slides smoothly and quantises only on release; without this
    // the step size fights the drag.
    snapMode: Slider.SnapOnRelease
    // scale:disabled { filter: Opacity(var(--disabled-opacity)) }
    opacity: enabled ? 1.0 : 0.5

    // _scale.scss: the modern Adwaita slider is chunky. The trough is a 10px
    // pill of currentColor 15% with the accent highlight inside it; the
    // control resizes its background item, so the trough is a child.
    background: Item {
        Rectangle {
            objectName: "gnome-slider-trough"
            x: root.leftPadding
            y: Math.round((parent.height - height) / 2)
            width: root.availableWidth
            height: 10
            radius: height / 2
            color: root.tokens.dark
                   ? Qt.rgba(1, 1, 1, root.tokens.highContrast ? 0.30 : 0.15)
                   : Qt.rgba(0, 0, 6 / 255,
                             root.tokens.highContrast ? 0.30 : 0.15)
            border.width: root.tokens.highContrast ? 1 : 0
            border.color: root.tokens.border

            Rectangle {
                width: root.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: root.tokens.accent
            }
        }
    }

    // The 20px round knob: color-mix(white 80%, --view-bg-color), ringed by
    // ---slider-border-color (black/6 at 10%, 35% under raised contrast) over
    // a 0 2px 4px black/20% drop.
    handle: Item {
        x: root.leftPadding
           + root.visualPosition * (root.availableWidth - width)
        y: Math.round((root.height - height) / 2)
        implicitWidth: 20
        implicitHeight: implicitWidth

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            radius: width / 2
            color: Qt.rgba(0, 0, 6 / 255, 0.2)
            z: -1
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Qt.rgba(0.8 + 0.2 * root.tokens.view.r,
                           0.8 + 0.2 * root.tokens.view.g,
                           0.8 + 0.2 * root.tokens.view.b, 1)
            border.width: root.visualFocus ? root.tokens.focusWidth : 1
            border.color: root.visualFocus
                          ? root.tokens.focus
                          : Qt.rgba(0, 0, 6 / 255,
                                    root.tokens.highContrast ? 0.35 : 0.10)
        }
    }

    Accessible.role: Accessible.Slider
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: enabled && focusPolicy !== Qt.NoFocus
    Accessible.focused: activeFocus
    Accessible.pressed: pressed
    Accessible.onIncreaseAction: {
        if (root.enabled) {
            root.increase()
        }
    }
    Accessible.onDecreaseAction: {
        if (root.enabled) {
            root.decrease()
        }
    }

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
