import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Starlight 1.0

// Collapsed-by-default reveal: an optional header row (chevron + label +
// hint) that toggles a height-animated content area below it. Introduced
// for the "Essentials + Advanced reveal" adaptive-settings redesign
// (docs/design/2026-07-14-per-host-settings.md's sibling design doc,
// 2026-07-18) -- SettingsView.qml and PcView.qml both use this to bury
// everything except a small essentials set behind one click, without
// deleting anything from reach.
//
// Ephemeral by design: `expanded` always starts false and there is no
// persistence of it across page visits. Both settings pages are pushed
// fresh (or destroyed-and-recreated) by the StackView on ordinary
// navigation, so a plain property initializer is sufficient -- essentials-
// first re-asserts itself every time the page is (re)entered. Do NOT wire
// this up to any persisted storage.
//
// Content stays vanilla: whatever's placed inside `SLDisclosure { ... }`
// (typically the same SLSectionHeader + spacer + SLGroup blocks
// SettingsView.qml/PcView.qml already use at the top level) binds directly
// to StreamingPreferences exactly as before -- this primitive never proxies,
// caches, or reaches into a value it reveals, and it never touches any
// child's own `topBorder` -- callers keep their existing `topBorder: false`
// on each group's first row unchanged (this wraps whole existing
// SLSectionHeader+SLGroup blocks verbatim, so those flags are already
// correct exactly as they are today).
//
// showHeader:false lets an external control drive `expanded` instead of
// this primitive's own header row -- used by PcView's per-host "Optimized"
// summary card, whose own "Adjust" link is the reveal affordance instead of
// a second, redundant header directly beneath it.
Item {
    id: root

    property string title: ""
    property string hint: ""
    property bool expanded: false
    property bool showHeader: true
    property string accessibleName: title
    property string accessibleDescription: hint
    readonly property Item accessibleItem: header

    default property alias content: bodyColumn.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: header.height + body.height

    // ── Header: the entire row is the hit target (not just the chevron) --
    // matches the HIG whole-label-clickable fix already applied elsewhere in
    // this app (see PcView.qml's addrChip / hostTitleEdit patterns). ──
    AbstractButton {
        id: header
        visible: root.showHeader
        width: parent.width
        height: root.showHeader ? 40 : 0
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        leftPadding: 15
        rightPadding: 15

        onClicked: root.expanded = !root.expanded

        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()

        Accessible.role: Accessible.Button
        Accessible.name: root.accessibleName
        Accessible.description: root.accessibleDescription
        Accessible.focusable: header.enabled && header.focusPolicy !== Qt.NoFocus
        Accessible.focused: header.activeFocus
        Accessible.pressed: header.pressed
        // Qt Quick has no exposed expanded state. A disclosure header is a
        // toggle button, so checkable/checked is the available truthful state
        // mapping instead of inventing an unsupported attached property.
        Accessible.checkable: true
        Accessible.checked: root.expanded
        Accessible.onPressAction: {
            if (header.enabled && root.showHeader) {
                header.clicked()
            }
        }
        Accessible.onToggleAction: {
            if (header.enabled && root.showHeader) {
                header.clicked()
            }
        }

        AccessibleIdentity {
            target: header
            stableId: root.objectName
        }

        background: Rectangle {
            radius: Theme.radiusChip
            color: (header.hovered || header.visualFocus) ? Theme.segBg : "transparent"
            border.width: header.visualFocus ? 2 : 0
            border.color: Theme.accent
        }

        contentItem: RowLayout {
            spacing: 8

            SLIcon {
                name: "chevron-down"
                tone: "ink"
                size: Theme.iconSmall
                rotation: root.expanded ? 0 : -90

                Behavior on rotation { NumberAnimation { duration: Theme.durationFast } }
            }

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.label2
                font.pixelSize: Theme.sizeCaption
                font.weight: Font.DemiBold
            }

            Text {
                text: root.hint
                visible: root.hint.length > 0
                color: Theme.label2
                font.pixelSize: Theme.sizeCaption
            }
        }
    }

    // ── Body: height-animated, clipped so mid-animation content doesn't
    // spill past the collapsed bounds. `enabled` (not just `visible`) tracks
    // `expanded` so collapsed content drops out of the Tab/gamepad focus
    // chain entirely -- Qt Quick's default focus-chain traversal skips
    // disabled items and their descendants, which is exactly "expanding
    // adds children to the chain, collapsing removes them" with no extra
    // bookkeeping. ──
    Item {
        id: body
        anchors.top: header.bottom
        width: parent.width
        clip: true
        height: root.expanded ? bodyOuter.implicitHeight : 0
        enabled: root.expanded
        visible: height > 0

        Behavior on height { NumberAnimation { duration: Theme.durationMedium; easing.type: Easing.OutCubic } }

        Column {
            id: bodyOuter
            width: parent.width
            spacing: 0

            Item { width: 1; height: Theme.spacingS }

            Column {
                id: bodyColumn
                width: parent.width
                spacing: 0
            }
        }
    }
}
