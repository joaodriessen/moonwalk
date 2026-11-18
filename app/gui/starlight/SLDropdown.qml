import QtQuick 2.9
// 2.5, not the file's previous 2.2: the Overlay attached type (Overlay.overlay,
// used below to clamp the popup to the window bounds) was only added in
// QtQuick.Controls 2.3 -- under the old-style versioned import an older tag
// doesn't just warn, it makes the type unresolvable ("ReferenceError: Overlay
// is not defined", confirmed via an offscreen smoke run). NavigableDialog.qml
// already uses 2.5 for the same reason.
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import Starlight 1.0
import "ControlCompat.js" as ControlCompat

// HIG popup-button: a Theme-colored chip that opens a themed list popup.
// Generalizes the resolution/frame-rate "chip + Menu" pattern already used
// inline in PcView.qml so SettingsView doesn't need to hand-roll a Menu (or
// keep the Material AutoResizingComboBox) for its dozen-plus combo settings,
// including long lists like Language that need to scroll.
//
// model: array of { text: string, value: var } -- value can be anything
// (a number, an enum constant, or a small object like {w, h}); the caller
// reads model[index].value in onActivated.
AbstractButton {
    id: root

    property var model: []
    property int currentIndex: -1
    property int popupMaxHeight: 300
    // Capped to the window's overlay width (minus a small margin) so a long
    // option list can never make the popup itself wider than the window --
    // otherwise the clamp logic below would have no valid position to
    // shift to on a narrow window.
    property real popupWidth: Math.min(Math.max(width, 200), Overlay.overlay ? Overlay.overlay.width - 8 : 400)

    readonly property var currentItem: (currentIndex >= 0 && currentIndex < model.length) ? model[currentIndex] : null
    readonly property string displayText: currentItem ? currentItem.text : ""
    // Empty means "use the current visual option" for standalone use. An
    // SLRow supplies the durable setting label when this is used in a form.
    property string accessibleName: ""
    property string accessibleDescription: ""
    readonly property bool popupOpen: popup.opened

    signal activated(int index)

    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    leftPadding: 10
    rightPadding: 8
    topPadding: 3
    bottomPadding: 3

    implicitWidth: label.implicitWidth + chevron.implicitWidth + leftPadding + rightPadding + 6
    implicitHeight: 24

    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleName.length > 0 ? accessibleName : displayText
    Accessible.description: accessibleDescription
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

    // Toggle: a second click on the chip closes the open popup (it used to
    // call open() again — the menu felt stuck to the chip).
    function togglePopup() {
        if (popup.opened) {
            popup.close()
        } else {
            popup.open()
        }
    }

    onClicked: togglePopup()
    Keys.onSpacePressed: togglePopup()
    Keys.onReturnPressed: togglePopup()
    Keys.onEnterPressed: togglePopup()

    background: Rectangle {
        radius: Theme.radiusChip
        color: Theme.segBg
        opacity: root.enabled ? 1.0 : 0.5
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? Theme.accent : Theme.hairline

        // Idle hover feedback on the closed chip (previously only the open
        // popup's rows highlighted on hover -- audit §4). Same faint
        // label-tinted overlay technique as SLButton's quiet variant:
        // Qt.lighter() reads oddly on a translucent segBg, so lift opacity
        // on a solid overlay instead of tinting the base color.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.hovered && root.enabled
            color: Theme.label
            opacity: 0.06
        }
    }

    contentItem: RowLayout {
        spacing: 4

        Text {
            id: label
            Layout.fillWidth: true
            text: root.displayText
            color: root.enabled ? Theme.label : Theme.label3
            font.pixelSize: Theme.sizeCaption
            elide: Text.ElideRight
        }

        SLIcon {
            id: chevron
            name: "chevron-down"
            size: 11
            opacity: root.enabled ? 0.55 : 0.3
            Layout.alignment: Qt.AlignVCenter
        }
    }

    Popup {
        id: popup

        width: root.popupWidth
        height: Math.min(root.popupMaxHeight, listView.contentHeight + topPadding + bottomPadding)
        padding: 4
        modal: true
        // modal gives us click-outside-to-close; dim explicitly OFF — washing
        // the whole window dark is sheet/dialog weight, wrong for an options
        // popup (and only implicitly absent today via the style default).
        dim: false
        focus: true
        // CloseOnPressOutside (not ...OutsideParent): any click elsewhere
        // dismisses, macOS-style; the chip itself toggles via togglePopup().
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationFast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.durationFast; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
        }

        // Positioned imperatively at open time (NavigableMenu's pattern):
        // Popup.x/y are relative to `root`, but the bound we care about is
        // the window overlay. This CANNOT be a declarative binding —
        // mapToItem() isn't dependency-tracked, so a binding evaluates once
        // at component creation (before layout even positions the chip) and
        // freezes; the popup then ignores the clamp and can bleed past the
        // window edge (seen live at minimum window width).
        onAboutToShow: {
            var overlayItem = Overlay.overlay
            if (!overlayItem) {
                x = 0
                y = root.height + 4
                return
            }
            var origin = root.mapToItem(overlayItem, 0, 0)
            var margin = 4

            var minOverlayX = margin
            var maxOverlayX = Math.max(minOverlayX, overlayItem.width - width - margin)
            x = Math.min(Math.max(origin.x, minOverlayX), maxOverlayX) - origin.x

            var below = root.height + 4
            if (origin.y + below + height <= overlayItem.height - margin) {
                y = below
            }
            else if (origin.y - 4 - height >= margin) {
                y = -4 - height
            }
            else {
                var minOverlayY = margin
                var maxOverlayY = Math.max(minOverlayY, overlayItem.height - height - margin)
                y = Math.min(Math.max(origin.y + below, minOverlayY), maxOverlayY) - origin.y
            }
        }

        onOpened: listView.forceActiveFocus()

        background: SLSheet {
            radius: Theme.radiusControl
            shadow: true
        }

        contentItem: ListView {
            id: listView

            implicitHeight: contentHeight
            clip: true
            model: root.model
            currentIndex: root.currentIndex
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true

            ScrollBar.vertical: SLScrollBar {}

            // Binding-safe: never self-write root.currentIndex here. The
            // caller's onActivated writes the pref, NOTIFY fires, and
            // root.currentIndex (bound via indexForValue(...)) recomputes.
            Keys.onReturnPressed: {
                root.activated(listView.currentIndex)
                popup.close()
            }
            Keys.onEnterPressed: {
                root.activated(listView.currentIndex)
                popup.close()
            }
            Keys.onEscapePressed: popup.close()

            delegate: ItemDelegate {
                id: delegateItem
                width: listView.width
                height: 30
                hoverEnabled: true
                // macOS menu semantics: the accent fill tracks the POINTER (or
                // keyboard position), while the CURRENT value is marked with a
                // checkmark in the gutter — matching NavigableMenuItem and how
                // native popup buttons read.
                highlighted: hovered || listView.currentIndex === index && listView.activeFocus

                readonly property bool isCurrent: index === root.currentIndex

                background: Rectangle {
                    radius: Theme.radiusChip
                    color: delegateItem.highlighted ? Theme.accent : "transparent"
                }

                contentItem: RowLayout {
                    spacing: 6

                    Text {
                        // Checkmark gutter — reserved even when unchecked so
                        // item text stays aligned (same pattern as
                        // NavigableMenuItem's gutter).
                        opacity: delegateItem.isCurrent ? 1 : 0
                        text: "✓"
                        color: delegateItem.highlighted ? Theme.accentInk : Theme.accent
                        font.pixelSize: Theme.sizeBody
                        font.bold: true
                        leftPadding: 4
                        Layout.preferredWidth: 16
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.text
                        color: delegateItem.highlighted ? Theme.accentInk : Theme.label
                        font.pixelSize: Theme.sizeBody
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                onClicked: {
                    // Binding-safe: don't self-write listView.currentIndex
                    // (bound to root.currentIndex) or root.currentIndex
                    // itself -- just emit intent and let the caller's pref
                    // write flow back through the binding.
                    root.activated(index)
                    popup.close()
                }
            }
        }
    }
}
