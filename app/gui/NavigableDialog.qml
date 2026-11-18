import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import Starlight 1.0
import "starlight"
import "adaptive/gnome" as AdaptiveGnome

Dialog {
    id: root

    // When set, the accept/yes button in the shared DialogButtonBox delegate
    // below renders as the route's destructive treatment instead of the
    // plain accent fill -- for irreversible actions (Remove Host). Inherited
    // by NavigableMessageDialog (its own footer/delegate reads this same
    // property via the QML scope chain, no need to redeclare it there).
    property bool destructiveAccept: false

    // Under the adaptive shell, shared dialogs wear the active platform's
    // dialog surface so they don't clash with the native pages beneath them.
    // Both properties are false in the default legacy shell, where the
    // adapter context properties are never set.
    readonly property bool gnomeChrome:
        (typeof gnomeDesktopAdapterActive !== "undefined"
         && gnomeDesktopAdapterActive === true)
        || (typeof adaptiveShellRequested !== "undefined"
            && adaptiveShellRequested === true && !plasmaChrome)
    readonly property bool plasmaChrome:
        typeof plasmaDesktopAdapterActive !== "undefined"
        && plasmaDesktopAdapterActive === true
    readonly property AdaptiveGnome.GnomeTokens gnomeTokens:
        AdaptiveGnome.GnomeTokens {}

    modal: true
    anchors.centerIn: Overlay.overlay

    padding: 22
    topPadding: 22
    bottomPadding: 4

    // Dimmed backdrop behind the sheet, per the mockup's .dim.
    // The dim must stay inside the window's client-side-decoration margin and
    // follow its corner radius; a full-bleed rectangle painted over the
    // transparent margin and squared off the rounded window corners.
    Overlay.modal: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: Window.window && Window.window.csdMargin !== undefined
                             ? Window.window.csdMargin : 0
            radius: Window.window && Window.window.csdRadius !== undefined
                    ? Window.window.csdRadius : 0
            color: root.gnomeChrome ? Qt.rgba(0, 0, 0, 0.32)
                                    : (root.plasmaChrome
                                       ? Qt.rgba(root.palette.shadow.r,
                                                 root.palette.shadow.g,
                                                 root.palette.shadow.b, 0.32)
                                       : Theme.sheetDim)
        }
    }

    // Shared hairline for the footer separator; subclasses' footers read it
    // through the scope chain like destructiveAccept.
    readonly property color dialogHairline:
        gnomeChrome ? gnomeTokens.border
                    : (plasmaChrome ? palette.mid
                                    : Theme.hairline)

    // One type-stable surface: stock SLSheet values in the legacy shell
    // (characterized by tst_stylebootstrap), platform dialog material when
    // an adapter is active.
    background: SLSheet {
        shadow: !root.gnomeChrome && !root.plasmaChrome
        radius: root.gnomeChrome
                ? root.gnomeTokens.radiusDialog
                : (root.plasmaChrome ? 6 : Theme.radiusCard)
        color: root.gnomeChrome
               ? root.gnomeTokens.dialogSurface
               : (root.plasmaChrome ? root.palette.window : Theme.cardSolid)
        border.width: root.gnomeChrome
                      ? (root.gnomeTokens.dark ? 1 : 0) : 1
        border.color: root.gnomeChrome
                      ? Qt.rgba(1, 1, 1, 0.1)
                      : (root.plasmaChrome ? root.palette.light
                                           : Theme.hairline)
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.durationMedium; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
    }

    // Themed action row. Content-only dialogs (customResDialog, customFpsDialog,
    // renamePcDialog, ...) get this for free via standardButtons; dialogs that
    // need extra footer behavior (NavigableMessageDialog's Help button) provide
    // their own footer and only need to reuse the delegate look.
    footer: DialogButtonBox {
        standardButtons: root.standardButtons
        visible: count > 0
        padding: 14
        spacing: 8
        alignment: Qt.AlignRight

        background: Item {
            Rectangle {
                // Hairline separating the button row from the body.
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: root.dialogHairline
            }
        }

        delegate: SLButton {
            // The accept/yes button reads as the accent-filled primary action
            // ("the button that does the thing"); everything else (Cancel/No/
            // Help) stays the plain quiet chip. A destructiveAccept dialog
            // (Remove Host) uses the app-owned danger treatment outside
            // Plasma; Plasma keeps the scheme's native highlight treatment.
            readonly property bool isAffirmative: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                                                   || DialogButtonBox.buttonRole === DialogButtonBox.YesRole

            quiet: !isAffirmative
            destructive: root.destructiveAccept && isAffirmative
                         && !root.plasmaChrome

            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
            Keys.onLeftPressed: nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
        }
    }

    onClosed: {
        // We must force focus back to the last item. If we don't,
        // gamepad and keyboard navigation will break after a
        // dialog appears.
        stackView.forceActiveFocus()
    }
}
