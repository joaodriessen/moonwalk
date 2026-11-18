import QtQuick 2.9
import Starlight 1.0
import "Lucide.js" as Lucide

// Theme-aware icon. Glyphs are Lucide's (res/lucide), drawn in the tone's
// colour on the CPU by LucideIconProvider rather than recoloured through a
// MultiEffect layer at runtime: GPU shader colorization proved fragile
// (icons rendered invisible on non-GPU render paths) and per-frame layers
// are pure overhead for what is a static image. Tones:
//   "ink"      — label color for the current palette (default)
//   "accent"   — accent blue for the current palette
//   "onAccent" — white, for glyphs sitting on accent-filled surfaces
//   "inkLight" — light-palette ink regardless of theme, for glyphs sitting on
//                a surface that stays light in both palettes (host-card box)
// The operating-system marks ("os_macos", "os_windows", "os_linux") are
// brand logos Lucide doesn't draw; they keep their pre-coloured SVGs in
// res/ (white) and res/tinted/.
Image {
    id: root

    // A Lucide glyph name, e.g. "settings", "chevron-down", or an OS mark.
    property string name
    property string tone: "ink"
    property int size: Theme.iconSmall
    // Scheme override for adaptive surfaces. The legacy Theme singleton and
    // the GNOME Appearance provider resolve dark mode independently; a GNOME
    // surface passes its own state here so glyphs cannot load the wrong-side
    // colour when the two diverge. Undefined keeps legacy behaviour exactly.
    property var schemeDark: undefined
    readonly property bool effectiveDark:
        schemeDark === undefined ? Theme.isDark : schemeDark === true
    readonly property bool brandMark: name.indexOf("os_") === 0
    readonly property color glyphColor: {
        if (tone === "onAccent") {
            return "#FFFFFF"
        }
        if (tone === "accent") {
            return effectiveDark ? "#0A84FF" : "#007AFF"
        }
        if (tone === "inkLight") {
            return "#232326"
        }
        return effectiveDark ? "#E6E6E8" : "#232326"
    }

    source: {
        if (!name.length) {
            return ""
        }
        if (!brandMark) {
            return Lucide.url(name, glyphColor)
        }
        if (tone === "onAccent") {
            return "qrc:/res/" + name + ".svg"   // the originals are baked white
        }
        var suffix
        if (tone === "accent") {
            suffix = effectiveDark ? "_accd" : "_accl"
        } else if (tone === "inkLight") {
            suffix = "_inkl"
        } else {
            suffix = effectiveDark ? "_inkd" : "_inkl"
        }
        return "qrc:/res/tinted/" + name + suffix + ".svg"
    }
    sourceSize.width: size
    sourceSize.height: size
    fillMode: Image.PreserveAspectFit
}
