import QtQuick 2.15

// Construction-only failure fixture. The component loads successfully, but
// Qt must reject createObject() because this required property is omitted.
Item {
    property string appName
    property var session
    property bool isResume: false
    property var quitSegueRequestedFn
    property var retainLaunchWarningsFn
    required property string requiredFailure
}
