import QtQuick 2.9
import QtQuick.Controls 2.2
import Starlight 1.0
import "ControlCompat.js" as ControlCompat

// macOS-style pill toggle. Built on AbstractButton so Space/Enter and
// Tab-order keyboard activation come for free (matches Navigable* focus
// conventions used throughout the rest of the app).
//
// Binding-safe: this control never writes its own `checked` property.
// Callers keep a declarative `checked: StreamingPreferences.x` binding and
// react to `switched(wanted)` by writing the pref -- the NOTIFY signal then
// re-evaluates the binding. (Previously checkable:true let AbstractButton
// self-write `checked` on click, permanently breaking that binding so the
// toggle stopped following host switches.)
AbstractButton {
    id: root

    checkable: false
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    // A toggle normally receives its visible label from SLRow.  Keep the
    // optional explicit input separate from the custom-painted visual so the
    // form container can supply that label without relying on its internals.
    property string accessibleName: text
    property string accessibleDescription: ""

    implicitWidth: 40
    implicitHeight: 24

    signal switched(bool wanted)

    onClicked: switched(!checked)

    // checkable:false still gets Space-to-click for free from AbstractButton,
    // but Return/Enter needs explicit wiring (same as SLDropdown).
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: root.enabled && root.focusPolicy !== Qt.NoFocus
    Accessible.focused: root.activeFocus
    Accessible.pressed: root.pressed
    Accessible.checkable: true
    Accessible.checked: root.checked
    // Both assistive actions honor enabled state, then use the ordinary
    // AbstractButton click route, preserving binding-safe switched(wanted).
    Accessible.onPressAction: {
        ControlCompat.activate(root)
    }
    Accessible.onToggleAction: {
        ControlCompat.activate(root)
    }

    AccessibleIdentity {
        target: root
        stableId: root.objectName
    }

    background: Rectangle {
        radius: height / 2
        color: root.checked ? Theme.ok : Theme.track
        opacity: root.enabled ? 1.0 : 0.45
        // Idle hover feedback (previously none -- audit §4): a faint
        // accentHover ring one step below the full 2px accent focus ring,
        // so hover and keyboard focus read as related-but-distinct states.
        border.width: root.visualFocus ? 2 : (root.hovered ? 1 : 0)
        border.color: root.visualFocus ? Theme.accent : Theme.accentHover

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

        Rectangle {
            width: parent.height - 4
            height: parent.height - 4
            radius: width / 2
            color: Theme.accentInk
            y: 2
            x: root.checked ? parent.width - width - 2 : 2

            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Item {}
}
