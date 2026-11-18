import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Templates 2.15 as T
import "../../starlight" as AppStyle

T.BusyIndicator {
    id: root

    // The active Qt Quick Controls style insets its backgrounds; the
    // Adwaita background drawn below must fill the control instead.
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    property string accessibleName: qsTr("Busy")
    property string accessibleDescription: ""
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property bool busyState: running
    readonly property bool animationRunning:
        running && !tokens.reducedMotion

    implicitWidth: 28
    implicitHeight: implicitWidth

    // AdwSpinner: a 15% currentColor track with a turning currentColor arc.
    contentItem: Canvas {
        id: ring
        implicitWidth: root.implicitWidth
        implicitHeight: root.implicitHeight
        visible: root.running
        readonly property color ink: root.tokens.text
        onInkChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            var stroke = Math.max(2, Math.round(Math.min(width, height) / 8))
            var radius = Math.min(width, height) / 2 - stroke / 2
            ctx.reset()
            ctx.lineWidth = stroke
            ctx.lineCap = "round"
            ctx.strokeStyle = Qt.rgba(ink.r, ink.g, ink.b, ink.a * 0.15)
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, radius, 0, 2 * Math.PI)
            ctx.stroke()
            ctx.strokeStyle = ink
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, radius, -Math.PI / 2, 0.1 * Math.PI)
            ctx.stroke()
        }
        RotationAnimator on rotation {
            running: root.animationRunning
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 1100
        }
    }

    Accessible.role: Accessible.Animation
    Accessible.name: accessibleName
    Accessible.description: accessibleDescription.length > 0
                            ? accessibleDescription
                            : (running ? qsTr("In progress") : qsTr("Idle"))

    AppStyle.AccessibleIdentity {
        target: root
        stableId: root.objectName
    }
}
