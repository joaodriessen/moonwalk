pragma Singleton
import QtQuick 2.15

// Rendering-only token adapter for child components that import Starlight.
// PcView itself continues to use its real local starlight/Theme.qml.
QtObject {
    readonly property SystemPalette _pal: SystemPalette {
        colorGroup: SystemPalette.Active
    }
    readonly property bool _palIsDark: (0.299 * _pal.window.r
                                        + 0.587 * _pal.window.g
                                        + 0.114 * _pal.window.b) < 0.5
    readonly property bool isDark: Qt.styleHints.colorScheme === 2
                                   || (Qt.styleHints.colorScheme !== 1 && _palIsDark)
    readonly property color accent: isDark ? "#0A84FF" : "#007AFF"
    readonly property color accentPress: isDark ? "#2F96FF" : "#0066D6"
    readonly property color accentHover: isDark ? "#1F8FFF" : "#0A85FF"
    readonly property color accentInk: "#FFFFFF"
    readonly property color ok: isDark ? "#30D158" : "#34C759"
    readonly property color danger: isDark ? "#FF453A" : "#FF3B30"
    readonly property color dangerPress: isDark ? "#FF6961" : "#D70015"
    readonly property color ground: isDark ? "#202024" : "#F6F6F8"
    readonly property color windowBg: isDark ? Qt.rgba(32/255, 32/255, 36/255, 0.94) : "#F6F6F8"
    readonly property color sidebar: isDark ? Qt.rgba(1, 1, 1, 0.045) : "#FFFFFF"
    readonly property color card: isDark ? Qt.rgba(1, 1, 1, 0.065) : "#FFFFFF"
    readonly property color cardSolid: isDark ? "#2A2A2E" : "#FFFFFF"
    readonly property color cardHeroTop: isDark ? "#2C2C31" : "#FFFFFF"
    readonly property color cardHeroBottom: isDark ? "#28282C" : "#FBFBFD"
    readonly property color hairline: isDark ? Qt.rgba(1, 1, 1, 0.11) : "#D0D0D0"
    readonly property color label: isDark ? Qt.rgba(1, 1, 1, 0.90) : "#202020"
    readonly property color label2: isDark ? Qt.rgba(1, 1, 1, 0.52) : "#707070"
    readonly property color label3: isDark ? Qt.rgba(1, 1, 1, 0.28) : "#A0A0A0"
    readonly property color track: isDark ? Qt.rgba(1, 1, 1, 0.14) : "#E0E0E0"
    readonly property color segBg: isDark ? Qt.rgba(1, 1, 1, 0.075) : "#F0F0F0"
    readonly property color sheetDim: isDark ? Qt.rgba(0, 0, 0, 0.35) : "#30000000"
    readonly property color darkChip: isDark ? "#3A3A40" : "#2E2E32"
    readonly property int sizeFootnote: 11
    readonly property int sizeCaption: 12
    readonly property int sizeBody: 13
    readonly property int sizeHeadline: 16
    readonly property int sizeLargeTitle: 25
    readonly property int iconSmall: 16
    readonly property int iconHero: 80
    readonly property int radiusCard: 12
    readonly property int radiusGroup: 10
    readonly property int radiusControl: 8
    readonly property int radiusChip: 6
    readonly property int spacingXS: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 14
    readonly property int spacingL: 20
    readonly property int spacingXL: 28
    readonly property int pageInsetX: 26
    readonly property int pageInsetTop: 22
    readonly property int dialogWidth: 340
    readonly property int durationFast: 120
    readonly property int durationMedium: 180
    readonly property int windowDefaultWidth: 860
    readonly property int windowDefaultHeight: 520
}
