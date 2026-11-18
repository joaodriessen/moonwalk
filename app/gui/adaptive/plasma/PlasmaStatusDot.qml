import QtQuick 2.15
import org.kde.kirigami 2.20 as Kirigami

// A host's state as a small dot in KDE's semantic colours, breathing while unknown.
Rectangle {
    id: root
    property string tone: "offline"
    property bool onHighlight: false
    implicitWidth: 8
    implicitHeight: implicitWidth
    radius: width / 2
    color: onHighlight ? Kirigami.Theme.highlightedTextColor
           : tone === "ready" ? Kirigami.Theme.positiveTextColor
           : tone === "active" ? Kirigami.Theme.linkColor
           : tone === "attention" ? Kirigami.Theme.neutralTextColor
           : Kirigami.Theme.disabledTextColor
    SequentialAnimation on opacity {
        running: root.tone === "checking" && root.visible && Kirigami.Units.longDuration > 0
        loops: Animation.Infinite
        NumberAnimation { to: 0.3; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
        onRunningChanged: if (!running) root.opacity = 1
    }
}
