import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Templates 2.15 as T
import QtQuick.Window 2.15
import Starlight 1.0

// GTK 4.22 gtkwindowcontrols + libadwaita 1.9 header-bar button contract.
Row {
    id: root

    property string side: "left"
    readonly property GnomeTokens tokens: GnomeTokens {}
    readonly property bool chromeActive:
        typeof gnomeDesktopAdapterActive !== "undefined"
        && gnomeDesktopAdapterActive === true
    readonly property string layoutSpec: Appearance.windowButtonLayout || ":close"
    readonly property var buttons: {
        if (side !== "left" && side !== "right") return []
        var halves = layoutSpec.split(":")
        var wanted = side === "left" ? halves[0] : (halves[1] || "")
        return wanted.split(",").filter(function(name) {
            return name === "close" || name === "minimize" || name === "maximize"
        })
    }

    visible: chromeActive && buttons.length > 0
    spacing: tokens.windowControlSpacing

    Repeater {
        // Gate on the logical condition, not on effective visibility: the
        // latter is false whenever an ancestor is hidden, which silently
        // empties the control set and collapses this row to zero width.
        model: root.chromeActive ? root.buttons : []

        delegate: T.ToolButton {
            id: control
            required property string modelData
            readonly property bool restoring: modelData === "maximize"
                && control.Window.window
                && control.Window.window.visibility === Window.Maximized
            // GNOME's own window glyphs, not Lucide: window controls belong to the desktop.
            readonly property string iconName: restoring
                ? "window-restore-symbolic" : "window-" + modelData + "-symbolic"

            objectName: "gnome-window-control-" + modelData
            contentItem: IconImage {
                anchors.centerIn: parent
                source: control.icon.source
                sourceSize: Qt.size(control.icon.width, control.icon.height)
                color: control.icon.color
            }
            width: root.tokens.windowControlHitSize
            height: width
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            padding: 0
            display: AbstractButton.IconOnly
            // Bundled symbolic glyphs: an icon theme or sandbox can never leave
            // a window control blank.
            icon.source: "qrc:/res/gnome/" + iconName + ".svg"
            icon.width: root.tokens.windowControlIconSize
            icon.height: root.tokens.windowControlIconSize
            icon.color: root.tokens.text

            Accessible.role: Accessible.Button
            Accessible.name: restoring ? qsTranslate("PcView", "Restore window")
                : (modelData === "close" ? qsTranslate("PcView", "Close window")
                : (modelData === "minimize" ? qsTranslate("PcView", "Minimize window")
                : qsTranslate("PcView", "Maximize window")))

            onClicked: {
                var win = control.Window.window
                if (!win) return
                if (modelData === "close") win.close()
                else if (modelData === "minimize") win.showMinimized()
                else win.visibility === Window.Maximized ? win.showNormal() : win.showMaximized()
            }

            background: Item {
                Rectangle {
                    anchors.centerIn: parent
                    width: root.tokens.windowControlImageSize
                    height: width
                    radius: width / 2
                    color: control.down ? root.tokens.controlPressed
                        : (control.hovered ? root.tokens.controlHover : root.tokens.control)
                }
            }
        }
    }
}
