import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import "../settings" as Settings

// Kirigami dialog anatomy for everything the shared HostDialogState can ask.
Item {
    id: root

    required property var page
    property Item focusReturnTarget: null
    anchors.fill: parent
    property var prepareFocusReturn: null
    readonly property Settings.HostDialogState state: Settings.HostDialogState {
        page: root.page
        onResponded: root.restoreFocus()
    }
    readonly property var effectiveOverlay: state.effectiveOverlay
    readonly property string visibleOverlayKind: state.kind
    readonly property bool readOnlyKind: state.readOnly
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
            if (root.prepareFocusReturn) root.prepareFocusReturn()
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
        else rejectButton.forceActiveFocus()
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

    PlasmaDialog {
        id: dialog
        objectName: root.state.semanticId
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: root.state.title
        width: Math.min(Kirigami.Units.gridUnit * (root.visibleOverlayKind === "presets" || root.visibleOverlayKind === "details" ? 30 : 24),
                        parent ? parent.width - Kirigami.Units.gridUnit * 2 : Kirigami.Units.gridUnit * 24)
        readonly property real fitHeight: parent ? parent.height - Kirigami.Units.gridUnit * 3 : 600
        accessibleName: title
        accessibleDescription: root.state.body + (root.state.pin ? " " + root.state.pin : "")
                               + (root.state.addHostError ? "\n" + root.state.addHostError : "")
        visible: false
        standardButtons: Dialog.NoButton
        onVisibleChanged: if (!visible) root.restoreFocus()
        onOpened: Qt.callLater(root.focusInitialControl)
        onClosed: root.restoreFocus()

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            Settings.PinDigits {
                objectName: "plasma-host-dialog-pin"
                Layout.alignment: Qt.AlignHCenter
                visible: root.visibleOverlayKind === "pairing"
                pin: root.state.pin
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                surface: Kirigami.Theme.backgroundColor
                ink: Kirigami.Theme.textColor
                outline: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
                cellRadius: Kirigami.Units.cornerRadius
                digitSize: Math.round(Kirigami.Theme.defaultFont.pointSize * 3.6)
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0 && root.visibleOverlayKind !== "details"
                text: root.state.body
                wrapMode: Text.WordWrap
            }
            RowLayout {
                visible: root.visibleOverlayKind === "pairing"
                spacing: Kirigami.Units.smallSpacing
                PlasmaBusyIndicator {
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: implicitWidth
                    running: parent.visible
                }
                Label {
                    text: qsTranslate("PcView", "Waiting for the host…")
                    opacity: 0.7
                }
            }
            ScrollView {
                id: detailsScroll
                objectName: "plasma-host-dialog-details-scroll"
                visible: root.visibleOverlayKind === "details"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(detailsForm.implicitHeight,
                    Math.max(48, dialog.fitHeight - detailsAddressEditor.implicitHeight - Kirigami.Units.gridUnit * 10))
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    id: detailsForm
                    width: detailsScroll.availableWidth
                    spacing: 0
                    Repeater {
                        model: root.state.detailRows
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.largeSpacing
                            Label {
                                Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                                text: modelData.label.length > 0 ? modelData.label + ":" : ""
                                horizontalAlignment: Text.AlignRight
                                opacity: 0.7
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.value
                                textFormat: Text.PlainText
                                wrapMode: Text.WrapAnywhere
                            }
                            ToolButton {
                                text: qsTranslate("PcView", "Copy %1").arg(modelData.label)
                                icon.name: "lucide-copy"
                                display: AbstractButton.IconOnly
                                focusPolicy: Qt.TabFocus
                                Accessible.name: text
                                ToolTip.visible: hovered
                                ToolTip.delay: Kirigami.Units.toolTipDelay
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
            Settings.ClipboardWriter { id: clipboard }
            ColumnLayout {
                id: detailsAddressEditor
                visible: root.visibleOverlayKind === "details"
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Label {
                    Layout.fillWidth: true
                    text: qsTranslate("PcView", "Connection address:")
                }
                PlasmaTextField {
                    id: detailsAddressField
                    objectName: "starlight.hosts.connection-address"
                    Layout.fillWidth: true
                    placeholderText: qsTranslate("PcView", "Automatic")
                    accessibleName: qsTranslate("PcView", "Connection address")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    onTextEdited: root.detailsAddressStatus = ""
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTranslate("PcView", "Blank uses the discovered address. A port is optional.")
                    wrapMode: Text.WordWrap
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    PlasmaButton {
                        objectName: "plasma-host-dialog-save-address"
                        text: qsTranslate("PcView", "Save")
                        accessibleName: qsTranslate("PcView", "Save Address")
                        icon.name: "lucide-save"
                        onClicked: root.saveDetailsAddress(detailsAddressField.text)
                    }
                    PlasmaButton {
                        objectName: "plasma-host-dialog-automatic-address"
                        text: qsTranslate("PcView", "Use Automatic")
                        onClicked: root.saveDetailsAddress("")
                    }
                }
                Label {
                    objectName: "plasma-host-dialog-address-status"
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.detailsAddressStatus
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }
            PlasmaBusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: root.visibleOverlayKind === "connection-test" && root.state.payload.dispatched === true
                running: visible
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.state.hasInput
                spacing: Kirigami.Units.smallSpacing
                PlasmaTextField {
                    id: primaryField
                    objectName: "plasma-host-dialog-primary-field"
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
                    opacity: 0.7
                }
                PlasmaTextField {
                    id: secondaryField
                    objectName: "plasma-host-dialog-secondary-field"
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
                objectName: "plasma-host-dialog-add-busy"
                visible: root.state.addHostBusy
                spacing: Kirigami.Units.smallSpacing
                PlasmaBusyIndicator {
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: implicitWidth
                    running: parent.visible
                }
                Label {
                    text: qsTranslate("PcView", "Looking for this host…")
                    opacity: 0.7
                }
            }
            Kirigami.InlineMessage {
                objectName: "plasma-host-dialog-add-error"
                Layout.fillWidth: true
                visible: root.state.addHostError.length > 0
                type: Kirigami.MessageType.Error
                text: root.state.addHostError
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.state.rangeNote
                font: Kirigami.Theme.smallFont
                opacity: 0.7
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            // Saved settings: name the current setup, then reuse or manage it.
            ColumnLayout {
                objectName: "plasma-host-dialog-presets"
                visible: root.visibleOverlayKind === "presets"
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing
                RowLayout {
                    Layout.fillWidth: true
                    PlasmaTextField {
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
                    PlasmaButton {
                        id: presetSave
                        objectName: "starlight.hosts.preset-save"
                        enabled: presetName.text.trim().length > 0
                        text: root.editingPresetId ? qsTranslate("PcView", "Rename") : qsTranslate("PcView", "Save")
                        icon.name: root.editingPresetId ? "lucide-pencil" : "lucide-save"
                        onClicked: {
                            var done = root.editingPresetId
                                     ? root.state.renamePreset(root.editingPresetId, presetName.text)
                                     : root.state.savePreset(presetName.text)
                            if (done) { presetName.text = ""; root.editingPresetId = "" }
                        }
                    }
                }
                Repeater {
                    model: root.visibleOverlayKind === "presets" ? root.page.presets : []
                    delegate: RowLayout {
                        required property var modelData
                        readonly property bool active: (root.page.hostPreferenceSnapshot || {}).activePresetName === modelData.name
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.Icon {
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: implicitWidth
                            source: parent.active ? "lucide-check" : ""
                        }
                        Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight; font.weight: parent.active ? Font.DemiBold : Font.Normal }
                        ToolButton {
                            objectName: "starlight.hosts.preset-apply." + modelData.id
                            enabled: !parent.active
                            text: qsTranslate("PcView", "Use")
                            icon.name: "lucide-check"
                            onClicked: { root.state.applyPreset(modelData.id, modelData.name); presetName.forceActiveFocus() }
                        }
                        ToolButton {
                            text: qsTranslate("PcView", "Rename")
                            icon.name: "lucide-pencil"
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            onClicked: { root.editingPresetId = modelData.id; presetName.text = modelData.name; presetName.forceActiveFocus() }
                        }
                        ToolButton {
                            text: qsTranslate("PcView", "Duplicate")
                            icon.name: "lucide-copy"
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            onClicked: root.state.duplicatePreset(modelData.id, modelData.name)
                        }
                        ToolButton {
                            objectName: "starlight.hosts.preset-remove." + modelData.id
                            text: qsTranslate("PcView", "Remove")
                            icon.name: "lucide-trash-2"
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            onClicked: root.state.removePreset(modelData.id, modelData.name)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: (root.page.presets || []).length === 0
                    text: qsTranslate("PcView", "No saved settings for this host yet.")
                    opacity: 0.7
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    objectName: "plasma-host-dialog-preset-status"
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.state.presetStatus
                    wrapMode: Text.WordWrap
                    Accessible.name: text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                Item { Layout.fillWidth: true }
                // KDE button order: the affirmative action sits left of Cancel.
                PlasmaButton {
                    id: acceptButton
                    objectName: "plasma-host-dialog-accept"
                    visible: !root.readOnlyKind && root.visibleOverlayKind !== "pairing"
                    enabled: visible && root.state.canAccept
                    highlighted: !root.state.destructive
                    icon.name: root.state.destructive ? "lucide-triangle-alert" : ""
                    text: root.state.acceptLabel
                    accessibleName: text
                    onClicked: root.acceptCurrent()
                    Keys.onReturnPressed: root.acceptCurrent()
                    Keys.onEnterPressed: root.acceptCurrent()
                }
                PlasmaButton {
                    id: rejectButton
                    objectName: "plasma-host-dialog-reject"
                    text: root.state.rejectLabel
                    icon.name: "lucide-x"
                    accessibleName: text
                    onClicked: root.rejectCurrent()
                    Keys.onReturnPressed: root.rejectCurrent()
                    Keys.onEnterPressed: root.rejectCurrent()
                }
            }
        }

        // Escape dismisses through rejectCurrent(); closePolicy stays
        // NoAutoClose so Popup can't bypass the overlay state machine.
        Shortcut {
            sequence: "Escape"
            enabled: dialog.visible
            onActivated: root.rejectCurrent()
        }
    }
}
