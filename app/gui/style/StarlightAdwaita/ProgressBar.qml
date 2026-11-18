import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

// GtkProgressBar: an 8px trough with the accent fill.
T.ProgressBar {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: 200
    implicitHeight: 8
    background: Rectangle {
        radius: height / 2
        color: control.tokens.control
    }
    contentItem: Item {
        Rectangle {
            width: control.indeterminate ? parent.width / 3 : control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: control.tokens.accent
            SequentialAnimation on x {
                running: control.indeterminate && control.visible && !control.tokens.reducedMotion
                loops: Animation.Infinite
                NumberAnimation { from: 0; to: control.availableWidth * 2 / 3; duration: 900; easing.type: Easing.InOutQuad }
                NumberAnimation { from: control.availableWidth * 2 / 3; to: 0; duration: 900; easing.type: Easing.InOutQuad }
            }
        }
    }
}
