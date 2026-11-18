import QtQml 2.15
import "generic" as Generic
import "gnome" as Gnome
import "plasma" as Plasma

QtObject {
    id: root

    // Supplied only by main.cpp after the pre-QApplication desktop, session,
    // CLI, style-module, and plugin-load gates all pass. `typeof` keeps QML
    // characterization harnesses and older embedding callers on the generic
    // route when that context property is absent.
    readonly property bool plasmaActive:
        typeof plasmaDesktopAdapterActive !== "undefined"
        && plasmaDesktopAdapterActive === true
    readonly property bool gnomeActive:
        typeof gnomeDesktopAdapterActive !== "undefined"
        && gnomeDesktopAdapterActive === true

    readonly property QtObject genericAdapter: Generic.GenericPlatformAdapter {}
    readonly property QtObject gnomeAdapter: Gnome.GnomePlatformAdapter {}
    readonly property QtObject plasmaAdapter: Plasma.PlasmaPlatformAdapter {}
    readonly property QtObject selectedAdapter:
        plasmaActive
        ? plasmaAdapter
        : (gnomeActive ? gnomeAdapter : genericAdapter)
    readonly property url hostRoute: selectedAdapter.hostRoute
    readonly property url settingsRoute: selectedAdapter.settingsRoute

}
