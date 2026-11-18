import QtQuick 2.15
import QtQuick.Controls.Material 2.15

import Starlight 1.0

// Loaded only when Material is the one globally selected Controls style. The
// environment keeps the legacy fallback defaults, while these inherited
// bindings preserve the live semantic light/dark and accent behavior of the
// real application window and its remaining raw Material delegates.
Item {
    id: root

    visible: false
    width: 0
    height: 0

    property var target: null
    readonly property var attached:
        target ? target.Material : null

    Binding {
        target: root.attached
        property: "theme"
        value: Theme.isDark ? Material.Dark : Material.Light
        when: root.attached !== null
    }

    Binding {
        target: root.attached
        property: "accent"
        value: Theme.accent
        when: root.attached !== null
    }
}
