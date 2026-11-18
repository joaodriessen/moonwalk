pragma Singleton
import QtQuick 2.15

QtObject {
    property int colorScheme: 1
    readonly property bool dark: colorScheme === 2
    property color accentColor: "#3584e4"
    property bool highContrast: false
    property real fontScale: 1.0
    property bool reducedMotion: false
    // Mirror the runtime AppearanceProvider's GNOME settings surface so the
    // adaptive chrome binds cleanly under test (no undefined assignments).
    property string windowButtonLayout: ":close"
    property string interfaceFontName: ""
    // Resolved pixel size of the desktop's interface font; 0 when there is
    // nothing to follow, which is the legacy/test default.
    property int interfaceFontPixelSize: 0

    function reset() {
        interfaceFontPixelSize = 0
        colorScheme = 1
        accentColor = "#3584e4"
        highContrast = false
        fontScale = 1.0
        reducedMotion = false
        windowButtonLayout = ":close"
        interfaceFontName = ""
    }
}
