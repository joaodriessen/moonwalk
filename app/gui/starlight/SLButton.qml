import QtQuick 2.9
import QtQuick.Controls 2.2
import Starlight 1.0
import "ControlCompat.js" as ControlCompat

// Accent pill button (primary) or "quiet" secondary variant, per the mockup's
// .btn / .btn.quiet styles. A `destructive` variant paints the primary path red
// (Theme.danger) for irreversible actions like Remove Host — before this, every
// confirmation button (benign or destructive) looked identical.
Button {
    id: root

    property bool quiet: false
    property bool destructive: false
    // Flagship hero host-card action (2026-07-18): a larger-presence
    // primary variant for the host card's single centered action slot
    // (Connect/Resume/Pair/Wake host). Purely additive -- every existing
    // call site is unaffected since it defaults false. ~44px tall,
    // >=180px wide, 15px DemiBold.
    property bool hero: false

    // Public semantic inputs deliberately live beside the control contract,
    // rather than being inferred from the themed contentItem.  A caller can
    // therefore keep an automation identifier and an assistive label stable
    // even when a visual adapter changes.
    property string accessibleName: text
    property string accessibleDescription: ""

    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription
    // Visibility is handled separately by Qt's accessibility tree. Keep the
    // focusable contract about the control's actual input capability so an
    // assistive technology's virtual focus is not coupled to its skin.
    Accessible.focusable: root.enabled && root.focusPolicy !== Qt.NoFocus
    Accessible.focused: root.activeFocus
    Accessible.pressed: root.pressed
    Accessible.onPressAction: {
        ControlCompat.activate(root)
    }

    AccessibleIdentity {
        target: root
        stableId: root.objectName
    }

    topPadding: hero ? 13 : (quiet ? 7 : 10)
    bottomPadding: hero ? 13 : (quiet ? 7 : 10)
    leftPadding: hero ? 30 : (quiet ? 14 : 26)
    rightPadding: hero ? 30 : (quiet ? 14 : 26)

    implicitWidth: Math.max(hero ? 180 : (quiet ? 0 : 130), contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    // Base fill by variant; hover/press derive from it so all variants get the
    // same feedback shape (the button previously had no hover state at all).
    readonly property color _baseColor: quiet ? Theme.segBg
                                              : (destructive ? Theme.danger : Theme.accent)
    readonly property color _pressColor: quiet ? Theme.track
                                              : (destructive ? Theme.dangerPress : Theme.accentPress)

    background: Rectangle {
        radius: root.quiet ? Theme.radiusChip : Theme.radiusControl
        color: root.pressed ? root._pressColor
                            : (root.hovered && !root.quiet ? Qt.lighter(root._baseColor, 1.08)
                                                           : root._baseColor)
        opacity: root.enabled ? 1.0 : 0.5
        border.width: root.visualFocus ? 2 : 0
        border.color: root.destructive ? Theme.danger : Theme.accent

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }

        // Quiet variant: a faint label-tinted overlay on hover (Qt.lighter on a
        // translucent segBg reads oddly, so lift opacity instead of tinting).
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.quiet && root.hovered && !root.pressed
            color: Theme.label
            opacity: 0.06
        }
    }

    contentItem: Text {
        text: root.text
        color: root.quiet ? Theme.label : Theme.accentInk
        font.pixelSize: root.quiet ? Theme.sizeBody : (root.hero ? 15 : 14)
        font.weight: root.quiet ? Font.Medium : Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
