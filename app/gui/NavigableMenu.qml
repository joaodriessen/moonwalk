import QtQuick 2.0
// 2.5, not 2.2: onAboutToShow below reads Overlay.overlay to clamp the menu
// to the window bounds -- that attached type needs QtQuick.Controls 2.3+ to
// resolve at all under the old-style versioned import (see the identical fix
// in SLDropdown.qml, confirmed via an offscreen smoke run: 2.2 throws
// "ReferenceError: Overlay is not defined" instead of just being an older API
// surface). NavigableDialog.qml already uses 2.5 for the same reason.
import QtQuick.Controls 2.5

import Starlight 1.0
import "starlight"

Menu {
    id: control

    property var initiator

    // Render in-scene, NOT as a native popup window. Qt 6.9+ defaults Menu to
    // Popup.Native where the platform supports it (Wayland does), which throws
    // away this menu's SLSheet/NavigableMenuItem theming in favor of a foreign
    // system-styled popup — and renders an empty husk on platforms without
    // native-popup support (seen live on the VNC harness).
    Component.onCompleted: {
        // popupType arrived in Qt 6.8. Item was the only implementation on
        // older Qt, so those versions already have the desired behavior.
        if ("popupType" in control) {
            control["popupType"] = Popup["Item"]
        }
    }

    padding: 6
    topPadding: 6
    bottomPadding: 6

    // Deterministic width: the widest item's own measure, floored so short
    // menus don't look stubby. Menu's built-in implicit sizing under Qt 6.11
    // collapsed with custom-contentItem items (see NavigableMenuItem).
    width: {
        var w = 180
        for (var i = 0; i < count; i++) {
            var it = itemAt(i)
            if (it && it.visible) {
                w = Math.max(w, it.implicitWidth + leftPadding + rightPadding)
            }
        }
        return Math.min(w, 320)
    }

    background: SLSheet {
        radius: Theme.radiusGroup
        shadow: true
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationFast; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.durationFast; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
    }

    // Menu.popup()/popup(x,y) position the menu imperatively (reparented to
    // Overlay.overlay, coordinates set via setPosition before the popup
    // becomes visible) -- a declarative x/y binding here would just get
    // clobbered by that assignment. So containment has to happen
    // reactively instead: onAboutToShow fires after popup() has set its
    // initial position but before the menu is actually shown, so nudging
    // x/y here is a one-time corrective move, not a fight with a binding.
    // (width/height already reflect the menu's real content size at this
    // point -- they're plain implicit-size layout, unaffected by
    // visibility -- so this isn't racing an unfinished layout pass.)
    onAboutToShow: {
        var overlayItem = Overlay.overlay
        if (!overlayItem) {
            return
        }

        var margin = 4
        var maxX = Math.max(margin, overlayItem.width - width - margin)
        var maxY = Math.max(margin, overlayItem.height - height - margin)

        if (x > maxX) {
            x = maxX
        }
        if (x < margin) {
            x = margin
        }
        if (y > maxY) {
            y = maxY
        }
        if (y < margin) {
            y = margin
        }
    }

    onOpened: {
        // If the initiating object currently has keyboard focus,
        // give focus to the first visible and enabled menu item
        if (initiator.focus) {
            for (var i = 0; i < count; i++) {
                var item = itemAt(i)
                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.TabFocusReason)
                    break
                }
            }
        }
    }
}
