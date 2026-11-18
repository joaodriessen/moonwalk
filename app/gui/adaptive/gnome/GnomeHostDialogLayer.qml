import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".." as Shared
import "../settings" as Settings

// AdwAlertDialog anatomy for everything the shared HostDialogState can ask.
Item {
    id: root

    required property var page
    property Item focusReturnTarget: null
    anchors.fill: parent
    readonly property Settings.HostDialogState state: Settings.HostDialogState {
        page: root.page
        onResponded: root.restoreFocus()
    }
    readonly property var effectiveOverlay: state.effectiveOverlay
    readonly property string visibleOverlayKind: state.kind
    readonly property bool readOnlyKind: state.readOnly
    readonly property bool closesInHeader: visibleOverlayKind === "details" || visibleOverlayKind === "presets"
    readonly property Item dismissControl: closesInHeader ? dialog.headerCloseButton : rejectButton
    property int initializedInputOperationId: -1
    property string initializedInputKind: ""
    property int detailsDraftOperationId: -1
    property string detailsAddressStatus: ""
    property string editingPresetId: ""

    function initializeDetailsAddress() {
        var overlay = page.baseOverlay
        if (!overlay || overlay.kind !== "details"
                || overlay.operationId === detailsDraftOperationId) return
        detailsDraftOperationId = overlay.operationId
        detailsAddressField.text = String((overlay.payload || {}).connectionAddress || "")
        detailsAddressStatus = ""
    }
    function saveDetailsAddress(value) {
        if (visibleOverlayKind !== "details"
                || effectiveOverlay.operationId !== detailsDraftOperationId) return false
        if (!page.submitConnectionAddress(detailsDraftOperationId, value)) {
            detailsAddressStatus = qsTranslate("PcView", "Address not saved. Check the address or reopen this host’s details.")
            return false
        }
        detailsAddressField.text = value.trim()
        detailsAddressStatus = value.trim().length > 0
                ? qsTranslate("PcView", "Address saved.") : qsTranslate("PcView", "Automatic connection enabled.")
        return true
    }
    Connections {
        target: root.page
        function onBaseOverlayChanged() { root.initializeDetailsAddress() }
    }
    function initializeFields() {
        if (!state.hasInputNow()) return
        var overlay = state.inputKind()
        if (overlay.operationId === initializedInputOperationId
                && overlay.kind === initializedInputKind) return
        initializedInputOperationId = overlay.operationId
        initializedInputKind = overlay.kind
        primaryField.text = state.initialText()
        secondaryField.text = state.initialSecondaryText()
    }
    function restoreFocus() {
        Qt.callLater(function() {
            // Guard on enabled rather than effective visibility: narrow-mode
            // panes disable their hidden side, and effective visibility is
            // also false under invisible harness roots where focus is legal.
            if (root.visibleOverlayKind === "none" && root.focusReturnTarget
                    && root.focusReturnTarget.enabled)
                root.focusReturnTarget.forceActiveFocus()
        })
    }
    function focusInitialControl() {
        if (state.destructive) rejectButton.forceActiveFocus()
        else if (primaryField.visible && primaryField.enabled) primaryField.forceActiveFocus()
        else if (presetName.visible) presetName.forceActiveFocus()
        else if (acceptButton.visible && acceptButton.enabled) acceptButton.forceActiveFocus()
        else root.dismissControl.forceActiveFocus()
    }
    onVisibleOverlayKindChanged: {
        if (visibleOverlayKind === "none") {
            initializedInputOperationId = -1
            initializedInputKind = ""
        }
        if (visibleOverlayKind !== "presets") {
            editingPresetId = ""
            state.presetStatus = ""
        }
        initializeFields()
        if (visibleOverlayKind === "none") {
            dialog.close()
            restoreFocus()
            return
        }
        // Publish the body before the popup becomes focused. Otherwise Orca
        // announces it on entry and again for the queued description change.
        dialog.synchronizeAccessibility()
        dialog.open()
        Qt.callLater(function() {
            if (root.visibleOverlayKind !== "none")
                root.focusInitialControl()
        })
    }
    Component.onCompleted: {
        initializeDetailsAddress()
        initializeFields()
        if (visibleOverlayKind !== "none") {
            dialog.synchronizeAccessibility()
            dialog.open()
        }
    }
    function acceptCurrent() { state.acceptCurrent() }
    function rejectCurrent() { state.rejectCurrent() }

    GnomeDialog {
        id: dialog
        objectName: root.state.semanticId
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: root.state.title
        closeInHeader: root.closesInHeader
        closeObjectName: root.closesInHeader ? "gnome-host-dialog-reject" : "gnome-dialog-close"
        onCloseRequested: root.rejectCurrent()
        accessibleName: title
        accessibleDescription: root.state.body + (root.state.pin ? " " + root.state.pin : "")
                               + (root.state.addHostError ? "\n" + root.state.addHostError : "")
        visible: false
        standardButtons: Dialog.NoButton
        onVisibleChanged: if (!visible) root.restoreFocus()
        onOpened: Qt.callLater(root.focusInitialControl)
        width: !parent ? 420 : sheet ? parent.width - 2 * outerMargin
                     : Math.min(root.closesInHeader ? 520 : 420, parent.width - 2 * tokens.spacingXL)
        // Never taller than the window, whatever a dialog has to say.
        readonly property real fitHeight: parent ? parent.height - 48 : 600

        contentItem: ColumnLayout {
            spacing: dialog.tokens.spacingM

            // The PIN is the whole point of this dialog: make it unmissable.
            Settings.PinDigits {
                objectName: "gnome-host-dialog-pin"
                Layout.alignment: Qt.AlignHCenter
                visible: root.visibleOverlayKind === "pairing"
                pin: root.state.pin
                surface: dialog.tokens.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 6 / 255, 0.05)
                ink: dialog.tokens.text
                outline: dialog.tokens.highContrast ? dialog.tokens.border : "transparent"
                digitSize: Math.round(dialog.tokens.heroTitleSize * 1.5)
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0 && root.visibleOverlayKind !== "details"
                text: root.state.body
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: dialog.tokens.secondaryText
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: root.visibleOverlayKind === "pairing"
                spacing: dialog.tokens.spacingS
                GnomeBusyIndicator { implicitWidth: 16; running: parent.visible }
                Label {
                    text: qsTranslate("PcView", "Waiting for the host…")
                    color: dialog.tokens.secondaryText
                    font.pixelSize: dialog.tokens.subtitleSize
                }
            }
            Flickable {
                id: detailsScroll
                objectName: "gnome-host-dialog-details-scroll"
                visible: root.visibleOverlayKind === "details"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(detailsColumn.implicitHeight, Math.max(120, dialog.fitHeight - dialog.tokens.toolbarHeight - 2 * dialog.tokens.spacingL))
                contentHeight: detailsColumn.implicitHeight
                contentWidth: width
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: GnomeScrollBar { accessibleName: dialog.title }
                ColumnLayout {
                    id: detailsColumn
                    width: detailsScroll.width
                    spacing: dialog.tokens.spacingL
                    GnomeGroup {
                        id: detailsAddressEditor
                        Layout.fillWidth: true
                        title: qsTranslate("PcView", "Connection address")
                        description: root.detailsAddressStatus.length > 0
                                     ? root.detailsAddressStatus
                                     : qsTranslate("PcView", "Blank uses the discovered address. A port is optional.")
                        GnomeRowBase {
                            GnomeTextField {
                                id: detailsAddressField
                                objectName: "starlight.hosts.connection-address"
                                Layout.fillWidth: true
                                Layout.preferredWidth: detailsColumn.width
                                placeholderText: qsTranslate("PcView", "Automatic")
                                accessibleName: qsTranslate("PcView", "Connection address")
                                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                                onTextEdited: root.detailsAddressStatus = ""
                                Keys.onReturnPressed: root.saveDetailsAddress(text)
                                Keys.onEnterPressed: root.saveDetailsAddress(text)
                            }
                            GnomeButton {
                                objectName: "gnome-host-dialog-save-address"
                                text: qsTranslate("PcView", "Save")
                                onClicked: root.saveDetailsAddress(detailsAddressField.text)
                            }
                            GnomeButton {
                                objectName: "gnome-host-dialog-automatic-address"
                                flatStyle: true
                                text: qsTranslate("PcView", "Automatic")
                                accessibleName: qsTranslate("PcView", "Use Automatic")
                                onClicked: root.saveDetailsAddress("")
                            }
                        }
                    }
                    // Hidden so the group's description can carry the status for sighted use.
                    Label {
                        objectName: "gnome-host-dialog-address-status"
                        visible: false
                        text: root.detailsAddressStatus
                    }
                    GnomeGroup {
                        Layout.fillWidth: true
                        title: qsTranslate("PcView", "About This Host")
                        Repeater {
                            model: root.state.detailRows
                            delegate: GnomeRowBase {
                                required property var modelData
                                title: modelData.label
                                subtitle: modelData.value
                                GnomeHeaderButton {
                                    symbol: "copy"
                                    accessibleName: qsTranslate("PcView", "Copy %1").arg(modelData.label)
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 600
                                    ToolTip.text: qsTranslate("PcView", "Copy")
                                    onClicked: {
                                        clipboard.write(modelData.value)
                                        root.detailsAddressStatus = qsTranslate("PcView", "Copied %1.").arg(modelData.label)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Shared.FocusReveal { viewport: detailsScroll }
            Settings.ClipboardWriter { id: clipboard }
            GnomeBusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: root.visibleOverlayKind === "connection-test" && root.state.payload.dispatched === true
                running: visible
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.state.hasInput
                spacing: dialog.tokens.spacingS
                GnomeTextField {
                    id: primaryField
                    objectName: "gnome-host-dialog-primary-field"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    visible: root.state.hasInput
                    placeholderText: root.state.primaryPlaceholder
                    accessibleName: placeholderText
                    accessibleDescription: root.state.addHostError
                    inputMethodHints: root.visibleOverlayKind === "custom-resolution"
                                      || root.visibleOverlayKind === "custom-frame-rate"
                                      ? Qt.ImhDigitsOnly : Qt.ImhNone
                    onTextChanged: root.state.textValue = text
                    Keys.onReturnPressed: root.acceptCurrent()
                    Keys.onEnterPressed: root.acceptCurrent()
                }
                Label {
                    visible: secondaryField.visible
                    text: "×"
                    color: dialog.tokens.secondaryText
                    font.pixelSize: dialog.tokens.titleSize
                }
                GnomeTextField {
                    id: secondaryField
                    objectName: "gnome-host-dialog-secondary-field"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    visible: root.visibleOverlayKind === "custom-resolution"
                    placeholderText: root.state.secondaryPlaceholder
                    accessibleName: placeholderText
                    inputMethodHints: Qt.ImhDigitsOnly
                    onTextChanged: root.state.secondaryValue = text
                    Keys.onReturnPressed: root.acceptCurrent()
                    Keys.onEnterPressed: root.acceptCurrent()
                }
            }
            RowLayout {
                objectName: "gnome-host-dialog-add-busy"
                Layout.alignment: Qt.AlignHCenter
                visible: root.state.addHostBusy
                spacing: dialog.tokens.spacingS
                GnomeBusyIndicator { implicitWidth: 16; running: parent.visible }
                Label {
                    text: qsTranslate("PcView", "Looking for this host…")
                    color: dialog.tokens.secondaryText
                    font.pixelSize: dialog.tokens.subtitleSize
                }
            }
            Label {
                objectName: "gnome-host-dialog-add-error"
                Layout.fillWidth: true
                visible: root.state.addHostError.length > 0
                text: root.state.addHostError
                wrapMode: Text.WordWrap
                color: dialog.tokens.destructiveText
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.state.rangeNote
                color: dialog.tokens.secondaryText
                font.pixelSize: dialog.tokens.subtitleSize
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            // Saved settings: name the current setup, then reuse or manage it.
            ColumnLayout {
                id: presetsEditor
                objectName: "gnome-host-dialog-presets"
                visible: root.visibleOverlayKind === "presets"
                Layout.fillWidth: true
                spacing: dialog.tokens.spacingM
                RowLayout {
                    Layout.fillWidth: true
                    spacing: dialog.tokens.spacingS
                    GnomeTextField {
                        id: presetName
                        objectName: "starlight.hosts.preset-name"
                        Layout.fillWidth: true
                        placeholderText: root.editingPresetId
                                         ? qsTranslate("PcView", "New name")
                                         : qsTranslate("PcView", "Name for the current settings")
                        accessibleName: placeholderText
                        Keys.onReturnPressed: presetSave.clicked()
                        Keys.onEnterPressed: presetSave.clicked()
                    }
                    GnomeButton {
                        id: presetSave
                        objectName: "starlight.hosts.preset-save"
                        suggested: presetName.text.trim().length > 0
                        enabled: presetName.text.trim().length > 0
                        text: root.editingPresetId ? qsTranslate("PcView", "Rename") : qsTranslate("PcView", "Save")
                        onClicked: {
                            var done = root.editingPresetId
                                     ? root.state.renamePreset(root.editingPresetId, presetName.text)
                                     : root.state.savePreset(presetName.text)
                            if (done) { presetName.text = ""; root.editingPresetId = "" }
                        }
                    }
                }
                GnomeGroup {
                    Layout.fillWidth: true
                    visible: (root.page.presets || []).length > 0
                    Repeater {
                        model: root.visibleOverlayKind === "presets" ? root.page.presets : []
                        delegate: GnomeRowBase {
                            id: presetRow
                            required property var modelData
                            readonly property bool active: (root.page.hostPreferenceSnapshot || {}).activePresetName === modelData.name
                            title: modelData.name
                            subtitle: active ? qsTranslate("PcView", "In use") : ""
                            GnomeButton {
                                flatStyle: !presetRow.active
                                enabled: !presetRow.active
                                objectName: "starlight.hosts.preset-apply." + modelData.id
                                text: qsTranslate("PcView", "Use")
                                // It disables itself once in use, so focus has to go somewhere.
                                onClicked: { root.state.applyPreset(modelData.id, modelData.name); presetName.forceActiveFocus() }
                            }
                            GnomeHeaderButton {
                                symbol: "edit"
                                accessibleName: qsTranslate("PcView", "Rename %1").arg(modelData.name)
                                ToolTip.visible: hovered
                                ToolTip.delay: 600
                                ToolTip.text: qsTranslate("PcView", "Rename")
                                onClicked: { root.editingPresetId = modelData.id; presetName.text = modelData.name; presetName.forceActiveFocus() }
                            }
                            GnomeHeaderButton {
                                symbol: "copy"
                                accessibleName: qsTranslate("PcView", "Duplicate %1").arg(modelData.name)
                                ToolTip.visible: hovered
                                ToolTip.delay: 600
                                ToolTip.text: qsTranslate("PcView", "Duplicate")
                                onClicked: root.state.duplicatePreset(modelData.id, modelData.name)
                            }
                            GnomeHeaderButton {
                                objectName: "starlight.hosts.preset-remove." + modelData.id
                                symbol: "trash"
                                accessibleName: qsTranslate("PcView", "Remove %1").arg(modelData.name)
                                ToolTip.visible: hovered
                                ToolTip.delay: 600
                                ToolTip.text: qsTranslate("PcView", "Remove")
                                onClicked: root.state.removePreset(modelData.id, modelData.name)
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: (root.page.presets || []).length === 0
                    text: qsTranslate("PcView", "No saved settings for this host yet.")
                    horizontalAlignment: Text.AlignHCenter
                    color: dialog.tokens.secondaryText
                }
                Label {
                    objectName: "gnome-host-dialog-preset-status"
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.state.presetStatus
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: dialog.tokens.secondaryText
                    Accessible.name: text
                }
            }
        }

        // Escape dismisses like the platform expects even though the overlay
        // state machine owns closing (closePolicy stays NoAutoClose so Popup
        // can't bypass rejectCurrent()).
        Shortcut {
            sequence: "Escape"
            enabled: dialog.visible
            onActivated: root.rejectCurrent()
        }

        // Adwaita alert response area (_message-dialog.scss, confirmed on a
        // real AdwAlertDialog): filled equal-width responses with 12px spacing
        // inside 24px padding (12px above), no hairlines.
        footer: Item {
            objectName: "gnome-host-dialog-footer"
            implicitHeight: root.closesInHeader ? 0 : responseRow.implicitHeight + dialog.tokens.spacingM + dialog.tokens.spacingXL

            RowLayout {
                id: responseRow
                visible: !root.closesInHeader
                anchors.fill: parent
                anchors.leftMargin: dialog.tokens.spacingXL
                anchors.rightMargin: dialog.tokens.spacingXL
                anchors.topMargin: dialog.tokens.spacingM
                anchors.bottomMargin: dialog.tokens.spacingXL
                spacing: dialog.tokens.spacingM

                GnomeDialogButton {
                    id: rejectButton
                    // fillWidth with equal preferred weight gives the native
                    // equal-width allocation regardless of label lengths.
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    objectName: root.closesInHeader ? "" : "gnome-host-dialog-reject"
                    text: root.state.rejectLabel
                    suggested: root.readOnlyKind
                    accessibleName: text
                    onClicked: root.rejectCurrent()
                    Keys.onReturnPressed: root.rejectCurrent()
                    Keys.onEnterPressed: root.rejectCurrent()
                }
                GnomeDialogButton {
                    id: acceptButton
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    objectName: "gnome-host-dialog-accept"
                    visible: !root.readOnlyKind && root.visibleOverlayKind !== "pairing"
                    enabled: visible && root.state.canAccept
                    suggested: !root.state.destructive
                    destructive: root.state.destructive
                    text: root.state.acceptLabel
                    accessibleName: text
                    onClicked: root.acceptCurrent()
                    Keys.onReturnPressed: root.acceptCurrent()
                    Keys.onEnterPressed: root.acceptCurrent()
                }
            }
        }
    }
}
