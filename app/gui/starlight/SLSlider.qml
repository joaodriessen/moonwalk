import QtQuick 2.9
import QtQuick.Controls 2.2
import Starlight 1.0

// Themed horizontal slider — a thin Theme.track rail, Theme.accent fill, and a
// round white handle with a hairline border. Extracted from PcView's inline,
// hand-styled bitrate Slider so a second slider can't regress to the unthemed
// Controls default. Binding-safe usage note is the caller's job (drive `value`
// imperatively when the backing property self-writes); this only owns the look.
Slider {
    id: root

    // A slider has no intrinsic visible label.  SLRow provides one for form
    // usage; standalone callers must provide these semantic inputs directly.
    property string accessibleName: ""
    property string accessibleDescription: ""

    // Needed for root.hovered below -- Slider doesn't default this on
    // (matches the explicit hoverEnabled every other SL* control sets).
    hoverEnabled: true

    // Qt's Slider accessibility bridge exposes from/to/value/stepSize from
    // the control itself. The attached properties below provide the missing
    // identity, focus state, and action routes for this custom skin.
    Accessible.role: Accessible.Slider
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    Accessible.focusable: root.enabled && root.focusPolicy !== Qt.NoFocus
    Accessible.focused: root.activeFocus
    Accessible.pressed: root.pressed
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

    AccessibleIdentity {
        target: root
        stableId: root.objectName
    }

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: 2
        color: Theme.track
        // Disabled: dim the whole rail (fill included, since it's a child)
        // -- previously a disabled SLSlider looked identical to an enabled
        // one (audit §4).
        opacity: root.enabled ? 1.0 : 0.45

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 18
        height: 18
        radius: 9
        color: Theme.accentInk
        opacity: root.enabled ? 1.0 : 0.45
        border.width: 1
        // Subtle hover feedback (previously none at all, audit §4): a
        // faint accentHover ring between rest (hairline) and pressed
        // (full accent) -- puts the declared-but-unused accentHover token
        // to work.
        border.color: root.pressed ? Theme.accent
                                   : (root.hovered ? Theme.accentHover : Theme.hairline)

        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }
}
