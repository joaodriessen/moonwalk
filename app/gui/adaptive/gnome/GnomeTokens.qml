import QtQuick 2.15
import Starlight 1.0

// App-owned Qt tokens matching the installed libadwaita 1.9 stylesheet.
// This is intentionally not a GTK/libadwaita widget implementation.
QtObject {
    readonly property bool dark: Appearance.dark
    readonly property bool highContrast: Appearance.highContrast
    readonly property real fontScale: Math.max(0.5,
                                               Math.min(3.0,
                                                        Appearance.fontScale))
    readonly property bool reducedMotion: Appearance.reducedMotion

    // Surfaces (libadwaita defaults-light/defaults-dark)
    readonly property color window: dark ? "#222226" : "#fafafb"
    readonly property color view: dark ? "#1d1d20" : "#ffffff"
    readonly property color headerbar: dark ? "#2e2e32" : "#ffffff"
    readonly property color sidebar: dark ? "#2e2e32" : "#ebebed"
    readonly property color card: dark ? Qt.rgba(1, 1, 1, 0.08) : "#ffffff"
    // A card's edge and lift are the three box-shadow layers of the .card
    // rule, which is why they carry no light/dark variant. The ring and
    // elevation colours are those layers; the drawn shadow is the asset
    // generated from them. cardBorder applies only under raised contrast,
    // where the source swaps the ring for a real border.
    readonly property color cardRing: Qt.rgba(0, 0, 6 / 255, 0.03)
    readonly property color cardShadow: Qt.rgba(0, 0, 6 / 255, 0.07)
    readonly property color cardBorder: border
    // --card-shade-color, the boxed-list row separator.
    readonly property color cardShade: dark ? Qt.rgba(0, 0, 6 / 255, 0.36)
                                            : Qt.rgba(0, 0, 6 / 255, 0.067)
    readonly property color dialogSurface: dark ? "#36363a" : "#fafafb"
    readonly property color popover: dark ? "#36363a" : "#ffffff"
    // Kept for pinned component APIs; maps to the raised popover surface.
    readonly property color surface: view
    readonly property color raisedSurface: popover

    // Foreground
    readonly property color text: dark ? "#ffffff" : Qt.rgba(0, 0, 6 / 255, 0.8)
    readonly property color secondaryText: dark
                                           ? Qt.rgba(1, 1, 1, 0.55)
                                           : Qt.rgba(0, 0, 6 / 255, 0.55)
    // --disabled-opacity is 50% in both schemes.
    readonly property color disabledText: dark
                                          ? Qt.rgba(1, 1, 1, 0.5)
                                          : Qt.rgba(0, 0, 6 / 255, 0.5)

    // Neutral fills: color-mix(currentColor N%, transparent) over any surface.
    readonly property color control: dark
                                     ? Qt.rgba(1, 1, 1, 0.10)
                                     : Qt.rgba(0, 0, 6 / 255, 0.10)
    readonly property color controlHover: dark
                                          ? Qt.rgba(1, 1, 1, 0.15)
                                          : Qt.rgba(0, 0, 6 / 255, 0.15)
    readonly property color controlPressed: dark
                                            ? Qt.rgba(1, 1, 1, 0.30)
                                            : Qt.rgba(0, 0, 6 / 255, 0.30)

    // Hairlines and separators
    readonly property color border: highContrast
                                    ? text
                                    : (dark ? Qt.rgba(1, 1, 1, 0.15)
                                            : Qt.rgba(0, 0, 6 / 255, 0.15))
    // --shade-color. Distinct from --headerbar-shade-color below, which is
    // the one that really is 36% in dark.
    readonly property color shade: dark
                                   ? Qt.rgba(0, 0, 6 / 255, 0.25)
                                   : Qt.rgba(0, 0, 6 / 255, 0.07)
    // Backdrop surfaces: GNOME repaints an unfocused window's header bar and
    // sidebar, which is why an inactive GTK window reads flatter than an
    // active one. --headerbar-backdrop-color / --sidebar-backdrop-color.
    readonly property color headerbarBackdrop: dark ? "#222226" : "#fafafb"
    readonly property color sidebarBackdrop: dark ? "#28282c" : "#f2f2f4"
    // --sidebar-border-color, the 1px rule between sidebar and content.
    readonly property color sidebarBorder: dark ? Qt.rgba(0, 0, 6 / 255, 0.36)
                                                : Qt.rgba(0, 0, 6 / 255, 0.07)
    // --scrollbar-outline-color keeps an overlay slider legible on any
    // background.
    readonly property color scrollbarOutline: dark ? Qt.rgba(0, 0, 12 / 255, 0.95)
                                                   : "#ffffff"
    // Undershoot indicator: --shade-color mixed at 75% (_drawing.scss).
    readonly property color undershoot: Qt.rgba(shade.r, shade.g, shade.b,
                                                shade.a * 0.75)
    readonly property color headerbarShade: dark
                                            ? Qt.rgba(0, 0, 6 / 255, 0.36)
                                            : Qt.rgba(0, 0, 6 / 255, 0.12)

    // Accent and statuses
    readonly property color accent: Appearance.accentColor
    readonly property color accentHover: Qt.lighter(accent, 1.08)
    readonly property color accentPressed: Qt.darker(accent, 1.12)
    readonly property color accentInk:
        (0.299 * accent.r + 0.587 * accent.g + 0.114 * accent.b) > 0.62
        ? "#1e1e1e" : "#ffffff"
    // --accent-color: the standalone accent for text, icons and focus, as
    // opposed to --accent-bg-color which fills buttons. libadwaita does not
    // lighten or darken; it clamps lightness in oklab and keeps the hue:
    //
    //   light: oklab(from @accent_bg_color min(l, 0.5) a b)
    //   dark:  oklab(from @accent_bg_color max(l, 0.85) a b)
    //
    // The accent arrives from the portal and can be any colour, so this is
    // computed. Out-of-gamut results are clipped per channel, which matches
    // what GTK produces to within one part in 255.
    readonly property color accentText: standaloneAccent(accent, dark)

    function standaloneAccent(source, isDark) {
        function toLinear(c) {
            return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)
        }
        function toSrgb(c) {
            var v = Math.max(0, Math.min(1, c))
            return v <= 0.0031308 ? v * 12.92
                                  : 1.055 * Math.pow(v, 1 / 2.4) - 0.055
        }
        function cubeRoot(v) {
            return v >= 0 ? Math.pow(v, 1 / 3) : -Math.pow(-v, 1 / 3)
        }

        var r = toLinear(source.r)
        var g = toLinear(source.g)
        var b = toLinear(source.b)

        var l = cubeRoot(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b)
        var m = cubeRoot(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b)
        var s = cubeRoot(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b)

        var okL = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s
        var okA = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s
        var okB = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s

        okL = isDark ? Math.max(okL, 0.85) : Math.min(okL, 0.5)

        var lp = okL + 0.3963377774 * okA + 0.2158037573 * okB
        var mp = okL - 0.1055613458 * okA - 0.0638541728 * okB
        var sp = okL - 0.0894841775 * okA - 1.2914855480 * okB
        lp = lp * lp * lp
        mp = mp * mp * mp
        sp = sp * sp * sp

        return Qt.rgba(
                    toSrgb(4.0767416621 * lp - 3.3077115913 * mp + 0.2309699292 * sp),
                    toSrgb(-1.2684380046 * lp + 2.6097574011 * mp - 0.3413193965 * sp),
                    toSrgb(-0.0041960863 * lp - 0.7034186147 * mp + 1.7076147010 * sp),
                    1)
    }
    readonly property color destructive: dark ? "#c01c28" : "#e01b24"
    readonly property color destructiveHover: dark ? "#a51d2d" : "#c01c28"
    readonly property color destructiveText: dark ? "#ff938c" : "#c30000"
    // --success-color, the standalone (text/icon) variant.
    readonly property color success: dark ? "#78e9ab" : "#007c3d"
    // --warning-color, the standalone (text/icon) variant.
    readonly property color warning: dark ? "#ffc252" : "#905300"
    // $focus_border_color mixes the STANDALONE accent at 50%, not the
    // background accent.
    readonly property color focus: highContrast
                                   ? text
                                   : Qt.rgba(accentText.r, accentText.g,
                                             accentText.b, 0.5)

    // Typography. libadwaita hardcodes no pixel sizes: every label is a
    // percentage of the desktop's interface font (_labels.scss), so the ramp
    // has to start from that font rather than assume one.
    //
    //   .title-1 181%   .title-2 136%   .title-3 136%   .title-4 118%
    //   .heading/.body 100%   .caption 82%   row subtitle `smaller` 83.33%
    //
    // 14 remains the fallback for the legacy shell, non-GNOME routes and
    // anywhere the desktop reports nothing to follow.
    readonly property int baseFontSize:
        Appearance.interfaceFontPixelSize > 0 ? Appearance.interfaceFontPixelSize
                                              : 14
    readonly property int captionSize: Math.max(
                                           9, Math.round(baseFontSize * 0.82 * fontScale))
    readonly property int subtitleSize: Math.max(
                                            9, Math.round(baseFontSize * 0.8333 * fontScale))
    readonly property int bodySize: Math.max(
                                        10, Math.round(baseFontSize * fontScale))
    readonly property int titleSize: Math.max(
                                         13, Math.round(baseFontSize * 1.18 * fontScale))
    readonly property int largeTitleSize: Math.max(
                                              15, Math.round(baseFontSize * 1.36 * fontScale))
    readonly property int heroTitleSize: Math.max(
                                             18, Math.round(baseFontSize * 1.81 * fontScale))

    // Metrics (Adwaita: buttons 9, cards/boxed lists 12, dialogs 15, pill 999)
    // A button's height is its content box's minimum plus its padding, not a
    // fixed total: _buttons.scss gives min-height 24 with 5px padding (34) and
    // .pill the same 24 with 10px padding (44).
    readonly property int buttonMinContentHeight: 24
    readonly property int buttonMinContentWidth: 16
    readonly property int controlHeight: 34
    readonly property int fieldHeight: 34
    // _lists.scss: row > box.header has min-height 50px.
    readonly property int rowMinHeight: 50
    readonly property int toolbarHeight: 46
    // libadwaita 1.9.2: _header-bar.scss, _buttons.scss. CSS pixels are
    // fixed logical widget geometry; font scaling must not multiply them.
    readonly property int headerbarHeight: 46
    readonly property int headerbarPaddingTop: 6
    readonly property int headerbarPaddingRight: 7
    readonly property int headerbarPaddingBottom: 6
    readonly property int headerbarPaddingLeft: 7
    readonly property int headerbarChildSpacing: 6
    readonly property int windowControlHitSize: 34
    readonly property int windowControlImageSize: 24
    readonly property int windowControlIconSize: 16
    readonly property int windowControlSize: windowControlHitSize
    readonly property int windowControlSpacing: 3
    // AdwWindowTitle reserves this even when its label is ellipsized;
    // measured off a native header bar. It is what stops a split view from
    // shrinking its sidebar until the title disappears.
    readonly property int windowTitleMinimumWidth: 69
    readonly property int radiusControl: 9
    readonly property int radiusCard: 12
    readonly property int radiusPopup: 15
    readonly property int radiusWindow: 15
    readonly property int radiusDialog: 15
    readonly property int radiusPill: 999
    readonly property int borderWidth: highContrast ? 2 : 1
    readonly property int focusWidth: highContrast ? 3 : 2
    readonly property int spacingXS: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 12
    readonly property int spacingL: 18
    readonly property int spacingXL: 24
    // AdwClamp's maximum-size on a preferences page.
    readonly property int contentClamp: 600
    readonly property int motionDuration: reducedMotion ? 0 : 160
}
