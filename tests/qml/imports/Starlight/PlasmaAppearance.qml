pragma Singleton
import QtQuick 2.15

QtObject {
    property bool ready: false
    property double revision: 0
    property var active: ({})
    property var inactive: ({})
    property var disabled: ({})
    property string widgetStyle: ""

    function reset() {
        ready = false
        revision = 0
        active = ({})
        inactive = ({})
        disabled = ({})
        widgetStyle = ""
    }
}
