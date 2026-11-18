import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2

import Starlight 1.0
import "starlight"
import "adaptive/gnome" as AdaptiveGnome

NavigableDialog {
    id: dialog

    property alias text: dialogLabel.dialogText
    property alias showSpinner: dialogSpinner.visible
    // Lucide glyph name (SLIcon) — "triangle-alert", "circle-check",
    // "circle-x", "circle-question-mark". Defaults by dialog kind
    // below. (Replaced the old imageSrc URL + MultiEffect-recolor pair; the
    // shader recoloring rendered glyphs invisible on non-GPU paths.)
    property string iconName: destructiveAccept ? "triangle-alert"
                              : (standardButtons & Dialog.Yes) ? "circle-question-mark" : "circle-x"

    property string helpText
    property string helpUrl : "https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting"
    property string helpTextSeparator : " "
    property bool scrollableBody: false
    // Dialog itself is a Qt Quick Controls Popup rather than an Item, so its
    // semantic content lives on the real RowLayout below.  Expose durable
    // public inputs here so callers can name an error/recovery dialog without
    // relying on the painted label or a style-specific dialog header.
    property string accessibleName: title
    property string accessibleDescription: dialogLabel.text
    readonly property Item accessibleItem: dialogContent

    onOpened: {
        // Force keyboard focus on the label so keyboard navigation works
        if (dialogButtonBox.count > 0) {
            dialogButtonBox.itemAt(dialogButtonBox.count - 1).forceActiveFocus(Qt.TabFocus)
        }
    }

    contentItem: RowLayout {
        id: dialogContent
        spacing: 14

        Accessible.role: Accessible.Dialog
        Accessible.name: dialog.accessibleName
        Accessible.description: dialog.accessibleDescription

        AccessibleIdentity {
            target: dialogContent
            stableId: dialog.objectName
        }

        SLBusy {
            id: dialogSpinner
            visible: false
            running: visible

            implicitWidth: 28
            implicitHeight: 28
        }

        SLIcon {
            name: dialog.iconName
            size: 28
            // Adwaita alerts carry no icon; the legacy sheet keeps its glyph.
            visible: !showSpinner && !dialog.gnomeChrome
            opacity: Theme.isDark ? 0.9 : 0.8
        }

        Label {
            property string dialogText

            id: dialogLabel
            visible: !dialog.scrollableBody
            objectName: dialog.objectName.length > 0 ? dialog.objectName + ".message" : ""
            text: dialogText + ((helpText && (standardButtons & Dialog.Help)) ? (helpTextSeparator + helpText) : "")
            wrapMode: Text.Wrap
            elide: Label.ElideRight
            color: dialog.gnomeChrome ? dialog.gnomeTokens.secondaryText
                                      : (dialog.plasmaChrome ? dialog.palette.windowText : Theme.label)
            font.pixelSize: dialog.gnomeChrome ? dialog.gnomeTokens.bodySize : Theme.sizeBody
            // Adwaita alert body is centred at the alert's narrow width.
            horizontalAlignment: dialog.gnomeChrome ? Text.AlignHCenter
                                                    : Text.AlignLeft
            Layout.preferredWidth: dialog.gnomeChrome ? 324 : -1

            // Cap the width so the dialog doesn't grow horizontally forever. This
            // will cause word wrap to kick in.
            Layout.maximumWidth: 400
            Layout.maximumHeight: 400

            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        ScrollView {
            id: messageScroll
            objectName: dialog.objectName.length > 0 ? dialog.objectName + ".scroll" : ""
            visible: dialog.scrollableBody
            Layout.preferredWidth: dialog.gnomeChrome ? 324 : 400
            Layout.preferredHeight: Math.min(scrollLabel.implicitHeight,
                Math.max(100, (dialog.parent ? dialog.parent.height : 600) - 220))
            contentWidth: availableWidth
            clip: true
            Label {
                id: scrollLabel
                width: messageScroll.availableWidth
                text: dialogLabel.text
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: dialog.gnomeChrome ? dialog.gnomeTokens.secondaryText
                                          : (dialog.plasmaChrome ? dialog.palette.windowText : Theme.label)
                font.pixelSize: dialog.gnomeChrome ? dialog.gnomeTokens.bodySize : Theme.sizeBody
                horizontalAlignment: dialog.gnomeChrome ? Text.AlignHCenter : Text.AlignLeft
            }
        }
    }

    footer: DialogButtonBox {
        id: dialogButtonBox
        standardButtons: dialog.standardButtons
        // Adwaita's response area: responses inside 24px padding (12 above)
        // with 12px spacing, stretched to equal widths (a DialogButtonBox
        // with no alignment fills the row). The legacy sheet keeps its
        // right-aligned 14/8 arrangement and hairline.
        padding: dialog.gnomeChrome ? 24 : 14
        topPadding: dialog.gnomeChrome ? 12 : 14
        spacing: dialog.gnomeChrome ? 12 : 8
        alignment: Qt.AlignRight

        // A DialogButtonBox creates its standard buttons with whatever
        // delegate it has at that moment; assigning the delegate later --
        // bound or in onCompleted -- does not rebuild them. Clearing and
        // restoring standardButtons after the swap forces the rebuild. The
        // chrome flag is immutable for the life of the shell.
        Component.onCompleted: {
            if (dialog.gnomeChrome || dialog.plasmaChrome) {
                delegate = dialog.gnomeChrome ? gnomeResponse : plasmaResponse
                if (dialog.gnomeChrome)
                    alignment = undefined
                var wanted = standardButtons
                standardButtons = DialogButtonBox.NoButton
                standardButtons = wanted
            }
        }

        background: Item {
            Rectangle {
                // Hairline separating the button row from the body.
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: dialog.dialogHairline
                visible: !dialog.gnomeChrome
            }
        }

        // The selected style's own button: Breeze under the Plasma adapter.
        Component {
            id: plasmaResponse

            Button {
                readonly property bool isAffirmative:
                    DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                    || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
                highlighted: isAffirmative && !dialog.destructiveAccept
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
            }
        }

        Component {
            id: gnomeResponse

            AdaptiveGnome.GnomeDialogButton {
                readonly property bool isAffirmative:
                    DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                    || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
                suggested: isAffirmative && !dialog.destructiveAccept
                destructive: dialog.destructiveAccept && isAffirmative

                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
                Keys.onLeftPressed: nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
            }
        }

        delegate: SLButton {
            // Same accept/yes-vs-cancel accent wiring as NavigableDialog's own
            // delegate above (kept in sync manually since this footer overrides
            // the base one entirely for the Help button below). destructiveAccept
            // is inherited straight from NavigableDialog -- no local redeclaration.
            readonly property bool isAffirmative: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                                                   || DialogButtonBox.buttonRole === DialogButtonBox.YesRole

            quiet: !isAffirmative
            destructive: dialog.destructiveAccept && isAffirmative

            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            Keys.onRightPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
            Keys.onLeftPressed: nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
        }

        onHelpRequested: {
            Qt.openUrlExternally(helpUrl)
            close()
        }
    }
}
