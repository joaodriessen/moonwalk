import QtQml 2.15

QtObject {
    readonly property url hostRoute:
        Qt.resolvedUrl("../HostPageRoute.qml")
    readonly property url settingsRoute:
        Qt.resolvedUrl("../SettingsPageRoute.qml")
}
