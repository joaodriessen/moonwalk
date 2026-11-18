import QtQuick 2.15
import QtQuick.Window 2.15
import "../../starlight" as AppStyle

// A host's identity: its OS glyph on a tile whose colour comes from the host
// itself, as AdwAvatar and Kirigami.Avatar colour people. Stable per host, so a
// list of computers can be told apart at a glance. Decorative: the row or
// heading beside it carries the name and state for assistive technology.
Item {
    id: root
    property string seed: ""
    property string iconName: "monitor"
    property int size: 40
    // An offline host keeps its shape and glyph but gives up its colour.
    property bool dimmed: false
    // A contrast-raised desktop gets a flat tile with an outline instead.
    property bool highContrast: false
    property color outline: "transparent"
    // That tile's glyph is ink, so it needs to know which way the desktop leans.
    property var schemeDark: undefined

    implicitWidth: size
    implicitHeight: size

    // libadwaita's avatar palette, without the yellow a white glyph cannot sit on.
    readonly property var ramps: [
        ["#62a0ea", "#1a5fb4"], ["#33c7c0", "#0e7c86"], ["#33d17a", "#1b8a54"],
        ["#ffa348", "#c64600"], ["#f66151", "#a51d2d"], ["#c061cb", "#613583"],
        ["#8f8bf0", "#4b43b5"], ["#d6885c", "#864f2e"]
    ]
    readonly property int rampIndex: {
        var hash = 5381
        for (var index = 0; index < seed.length; ++index)
            hash = ((hash * 33) ^ seed.charCodeAt(index)) >>> 0
        return hash % ramps.length
    }
    readonly property var ramp: dimmed ? ["#9a9996", "#5e5c64"] : ramps[rampIndex]
    // What a surface can tint itself with to belong to this host.
    readonly property color tint: ramp[1]

    Rectangle {
        id: tile
        anchors.fill: parent
        radius: Math.round(root.size * 0.27)
        gradient: root.highContrast ? null : fill
        color: root.highContrast ? "transparent" : root.ramp[1]
        border.width: root.highContrast ? Math.max(1, Math.round(root.size / 32)) : 0
        border.color: root.outline
        Gradient {
            id: fill
            GradientStop { position: 0; color: root.ramp[0] }
            GradientStop { position: 1; color: root.ramp[1] }
        }
    }
    // Pressed leather, like the app icon: colour-free layers over the host's own
    // gradient, so every ramp and the dimmed grey share them. Made by
    // scripts/press-host-avatars.py; plain images, no shader recolouring.
    readonly property string pressedGlyph:
        ["os_linux", "os_macos", "os_windows", "monitor"].indexOf(iconName) >= 0 ? iconName : ""
    readonly property bool pressed: !highContrast && pressedGlyph.length > 0
    readonly property int layerPixels: Math.min(256, Math.ceil(size * Screen.devicePixelRatio))
    Repeater {
        model: root.pressed ? ["grain-dark", "grain-light", root.pressedGlyph + "-dark", root.pressedGlyph + "-light"] : []
        Image {
            anchors.fill: parent
            source: "qrc:/res/avatar/" + modelData + ".png"
            sourceSize.width: root.layerPixels
            sourceSize.height: root.layerPixels
            smooth: true
            mipmap: true
        }
    }
    AppStyle.SLIcon {
        anchors.centerIn: parent
        visible: !root.pressed
        name: root.iconName
        // The originals are baked white for sitting on a coloured fill.
        tone: root.highContrast ? "ink" : "onAccent"
        schemeDark: root.schemeDark
        size: Math.round(root.size * 0.58)
    }
}
