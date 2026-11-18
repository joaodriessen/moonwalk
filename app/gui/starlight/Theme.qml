pragma Singleton
import QtQuick 2.9

// Starlight design tokens (HIG mockup: docs/design/2026-07-13-hig-mockup.html)
//
// Registered as a true QML singleton from main.cpp via:
//   qmlRegisterSingletonType(QUrl("qrc:/gui/starlight/Theme.qml"), "Starlight", 1, 0, "Theme")
//
// Light/dark follows the desktop's announced color scheme
// (Qt.styleHints.colorScheme, the Linux-native equivalent of
// "prefers-color-scheme"). Qt feeds it from the XDG settings portal on
// GNOME-family desktops (Qt >= 6.5) and updates it LIVE when the user
// toggles dark mode -- and it is the only signal that works inside a
// Flatpak, where the platform palette does NOT track the desktop scheme
// (measured: palette stays light on a dark GNOME session while
// colorScheme correctly reports Dark). Where the scheme is Unknown
// (older Qt, no portal, bare X11 sessions), fall back to inferring it
// from how dark the platform's window-background color is. Both
// branches below mirror the two :root blocks in the approved mockup
// exactly (same hex values).
QtObject {
    id: root

    readonly property SystemPalette _pal: SystemPalette { colorGroup: SystemPalette.Active }

    // (r*299 + g*587 + b*114) / 1000 is the standard perceptual luma weighting;
    // below the midpoint we treat the desktop as running a dark color scheme.
    readonly property bool _palIsDark: (0.299 * _pal.window.r + 0.587 * _pal.window.g + 0.114 * _pal.window.b) < 0.5

    // Qt::ColorScheme values: 0 Unknown, 1 Light, 2 Dark. Compared numerically
    // because the Qt.ColorScheme enum namespace doesn't exist in Qt 5's QML
    // engine (dereferencing it there throws), while colorScheme itself is
    // simply undefined pre-6.5 -- undefined safely fails both comparisons and
    // lands in the palette-luma fallback.
    readonly property bool isDark: Qt.styleHints.colorScheme === 2
                                   || (Qt.styleHints.colorScheme !== 1 && _palIsDark)

    // Startup + live-switch diagnostic; also confirms dark-mode tracking in
    // logs where a screenshot isn't possible (e.g. support reports).
    Component.onCompleted: console.info("Theme: resolved " + (isDark ? "dark" : "light")
                                        + " (desktop colorScheme=" + Qt.styleHints.colorScheme + ")")
    onIsDarkChanged: console.info("Theme: desktop color scheme changed; now " + (isDark ? "dark" : "light"))

    // Accent
    readonly property color accent: isDark ? "#0A84FF" : "#007AFF"
    readonly property color accentPress: isDark ? "#2F96FF" : "#0066D6"
    readonly property color accentHover: isDark ? "#1F8FFF" : "#0A85FF"
    readonly property color ok: isDark ? "#30D158" : "#34C759"

    // Destructive / danger (irreversible actions — Remove Host, etc.). Before this,
    // Theme had no red at all, so a Delete confirmation rendered pixel-identical to a
    // benign OK.
    readonly property color danger: isDark ? "#FF453A" : "#FF3B30"
    readonly property color dangerPress: isDark ? "#FF6961" : "#D70015"

    // Surfaces
    // Opaque window ground (no vibrancy) -- used behind full-window states
    // like the connect/quit segues that have no desktop content to blur.
    readonly property color ground: isDark ? "#202024" : "#F6F6F8"
    readonly property color windowBg: isDark ? Qt.rgba(32/255, 32/255, 36/255, 0.94) : Qt.rgba(246/255, 246/255, 248/255, 0.96)
    readonly property color sidebar: isDark ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(1, 1, 1, 0.38)
    readonly property color card: isDark ? Qt.rgba(1, 1, 1, 0.065) : Qt.rgba(1, 1, 1, 0.78)
    readonly property color cardSolid: isDark ? "#2A2A2E" : "#FFFFFF"
    // Whisper-subtle (~2%) vertical "lit-from-top" lift for the flagship hero
    // host card ONLY (owner-eyes flourish, design doc §5). Two-stop gradient,
    // top->bottom, no shader/MultiEffect involved -- Rectangle's own gradient
    // respects radius natively, so it stays clear of the documented
    // MultiEffect glyph-invisibility bug. Single consumer: hostCard.
    readonly property color cardHeroTop: isDark ? "#2C2C31" : "#FFFFFF"
    readonly property color cardHeroBottom: isDark ? "#28282C" : "#FBFBFD"
    readonly property color hairline: isDark ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(0, 0, 0, 0.10)

    // Labels
    readonly property color label: isDark ? Qt.rgba(1, 1, 1, 0.90) : Qt.rgba(0, 0, 0, 0.86)
    readonly property color label2: isDark ? Qt.rgba(1, 1, 1, 0.52) : Qt.rgba(0, 0, 0, 0.48)
    readonly property color label3: isDark ? Qt.rgba(1, 1, 1, 0.28) : Qt.rgba(0, 0, 0, 0.28)

    // Text/icon color for content sitting on an accent (or other saturated)
    // fill -- e.g. the selected sidebar row's name text, a primary button's
    // label, a highlighted dropdown row. Same idea as SLIcon's own
    // tone:"onAccent" (SLIcon.qml), which already picks a pre-tinted white
    // asset for icons on accent fills -- this is the color-token equivalent
    // for Text elements, replacing several places that each typed their own
    // literal "#ffffff". No isDark branch: unlike label/label2/label3 (which
    // scale against a light-or-dark page ground), this sits on a fill whose
    // own color already carries the light/dark distinction, so a single
    // constant is correct in both palettes (matches SLIcon's un-tinted
    // "baked white" onAccent asset, which also has no per-palette variant).
    readonly property color accentInk: "#FFFFFF"

    // Controls
    readonly property color track: isDark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.09)
    readonly property color segBg: isDark ? Qt.rgba(1, 1, 1, 0.075) : Qt.rgba(0, 0, 0, 0.055)
    readonly property color sheetDim: isDark ? Qt.rgba(0, 0, 0, 0.35) : Qt.rgba(0, 0, 0, 0.18)

    // Shared dark chip background for transient overlays (the bottom toast
    // and the app-wide ToolTip singleton in main.qml) -- was duplicated
    // verbatim as the same two hex literals in both places.
    readonly property color darkChip: isDark ? "#3A3A40" : "#2E2E32"

    // Type scale (px, matched to the mockup's 13/25/11-12 rhythm)
    readonly property int sizeFootnote: 11
    readonly property int sizeCaption: 12
    readonly property int sizeBody: 13
    readonly property int sizeHeadline: 16
    // HIG "Title 3" -- the hero host-name rung. Fills the 16->25 void: the
    // host name is the page's single hero identity and previously shared
    // sizeHeadline(16) with dialog titles, the Settings title, and every
    // section header, with no rung of its own. Scoped to exactly one call
    // site (the consolidated host card's name) -- no other size changed.
    readonly property int sizeTitle3: 20
    readonly property int sizeLargeTitle: 25

    // Radii
    readonly property int radiusCard: 12
    readonly property int radiusGroup: 10
    readonly property int radiusControl: 8
    readonly property int radiusChip: 6      // chips, dropdown fields, small hover pills

    // Spacing scale (px). Replaces the two-dozen ad-hoc `Item { width:1; height:N }`
    // spacers and raw `spacing:` literals that let PcView (20) and SettingsView (22)
    // silently drift out of rhythm. spacingL is THE canonical gap between form sections.
    readonly property int spacingXS: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 14
    readonly property int spacingL: 20
    readonly property int spacingXL: 28

    // Page content inset (was the 26 / 22 / 52 magic-number trio duplicated verbatim
    // across PcView.qml and SettingsView.qml — width uses parent.width - 2*pageInsetX).
    readonly property int pageInsetX: 26
    readonly property int pageInsetTop: 22

    // Fixed dialog width shared by every NavigableDialog with a compact
    // form (add host, pair sheet, rename, custom res/fps) -- was the bare
    // literal 340 repeated at each dialog's own `width:` instead of one
    // shared constant.
    readonly property int dialogWidth: 340

    // Icon size grid (retires the ad-hoc 13/14/18/26/30 sprinkled per call site).
    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int iconLarge: 24
    // Flagship hero host-card glyph (2026-07-18): the one large centered OS
    // mark atop the host card. Single consumer -- do not reuse for anything
    // smaller than a full-hero identity glyph.
    // 64 → 80 (owner + agent agreed 2026-07-19): at hero scale with no
    // container box, 64 read undersized against the large-title host name.
    readonly property int iconHero: 80

    // Motion (unifies the scattered 120/140/180ms literals in SLToggle/SLScrollBar/main).
    readonly property int durationFast: 120
    readonly property int durationMedium: 180

    // Launcher window default size. Single source for the 860×520 pair that main.qml
    // otherwise types in three independent places (width/height, the resolved*() fallbacks,
    // and the load-bearing StackView implicit-size pin) — the exact duplication pattern
    // behind the two already-fixed Wayland sizing bugs.
    readonly property int windowDefaultWidth: 880
    readonly property int windowDefaultHeight: 600
}
