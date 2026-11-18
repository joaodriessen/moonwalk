import QtQuick 2.15

// A host's state as a small coloured dot, gently breathing while it is unknown.
Rectangle {
    id: root
    property string tone: "offline"
    readonly property GnomeTokens tokens: GnomeTokens {}
    implicitWidth: 8
    implicitHeight: implicitWidth
    radius: width / 2
    color: tone === "ready" ? tokens.success
           : tone === "active" ? tokens.accentText
           : tone === "attention" ? tokens.warning
           : tokens.disabledText
    SequentialAnimation on opacity {
        running: root.tone === "checking" && root.visible && !root.tokens.reducedMotion
        loops: Animation.Infinite
        NumberAnimation { to: 0.3; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
        onRunningChanged: if (!running) root.opacity = 1
    }
}
