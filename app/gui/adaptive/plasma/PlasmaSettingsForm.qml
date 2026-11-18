import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import "../settings" as Settings

// Every shared SettingGroup in one Kirigami.FormLayout: the System Settings
// anatomy of right-aligned labels and controls, with sections and help text.
Kirigami.FormLayout {
    id: root
    required property var groups
    readonly property Settings.SettingRow placeholderRow: Settings.SettingRow { shown: false }

    function flatten(list) {
        var flat = []
        for (var g = 0; g < list.length; ++g) {
            flat.push({ section: list[g] })
            for (var r = 0; r < list[g].rows.length; ++r) {
                var row = list[g].rows[r]
                flat.push({ row: row })
                for (var n = 0; row.kind === "expander" && n < row.rows.length; ++n)
                    flat.push({ row: row.rows[n], parentRow: row })
            }
        }
        return flat
    }

    Repeater {
        model: root.flatten(root.groups)
        delegate: Loader {
            id: entry
            required property var modelData
            readonly property var section: modelData.section || null
            // Rows can be destroyed before this delegate when a page closes.
            readonly property var row: modelData.section ? null : (modelData.row || root.placeholderRow)
            readonly property var parentRow: modelData.parentRow || null
            readonly property bool revealed: !parentRow || parentRow.expanded === true
            // Check boxes and action buttons carry their own text, as in KCMs.
            readonly property bool labelled: !!row && (row.kind === "combo" || row.kind === "slider" || row.kind === "choice")
            Kirigami.FormData.isSection: !!section
            Kirigami.FormData.labelAlignment: !!row && row.kind === "choice" ? Qt.AlignTop : 0
            Kirigami.FormData.label: section ? section.title : (labelled ? row.title + ":" : "")
            visible: section ? section.shown === true && section.filteredOut !== true
                             : (row.shown === true && row.filteredOut !== true && revealed)
            Layout.fillWidth: !section
            Layout.maximumWidth: section ? -1 : Kirigami.Units.gridUnit * 24
            sourceComponent: section ? (section.description.length > 0 ? sectionNote : sectionGap)
                                     : row.kind === "combo" ? comboComponent
                                     : row.kind === "choice" ? choiceComponent
                                     : row.kind === "slider" ? sliderComponent
                                     : row.kind === "action" ? actionComponent
                                     : row.kind === "expander" ? expanderComponent
                                     : checkComponent

            Component { id: sectionGap; Item { implicitHeight: 1 } }
            Component {
                id: sectionNote
                Label {
                    objectName: "plasma-section-description"
                    text: entry.section.description
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                    wrapMode: Text.Wrap
                    Accessible.name: text
                }
            }
            Component {
                id: checkComponent
                ColumnLayout {
                    spacing: 2
                    PlasmaToggle {
                        Layout.fillWidth: true
                        objectName: entry.row.controlId
                        text: entry.row.title
                        accessibleName: entry.row.accessibleName
                        accessibleDescription: entry.row.accessibleDescription
                        enabled: entry.row.sensitive
                        checked: entry.row.checked
                        onToggled: entry.row.toggled(checked)
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing
                        visible: text.length > 0
                        text: entry.row.subtitle
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                        wrapMode: Text.Wrap
                    }
                    Label {
                        objectName: entry.row.controlId.replace("starlight.", "warning.")
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing
                        visible: text.length > 0
                        text: entry.row.warning
                        // System Settings' neutral tone: a consequence, not an error.
                        color: Kirigami.Theme.neutralTextColor
                        font: Kirigami.Theme.smallFont
                        wrapMode: Text.Wrap
                    }
                }
            }
            Component {
                id: comboComponent
                ColumnLayout {
                    spacing: 2
                    PlasmaDropdown {
                        id: combo
                        Layout.fillWidth: true
                        objectName: entry.row.controlId
                        accessibleName: entry.row.accessibleName
                        accessibleDescription: entry.row.accessibleDescription
                        enabled: entry.row.sensitive
                        model: entry.row.options
                        textRole: entry.row.textRole
                        currentIndex: entry.row.currentIndex
                        displayText: entry.row.displayText.length > 0 ? entry.row.displayText : currentText
                        onActivated: function(index) { entry.row.activated(index) }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: entry.row.subtitle
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                        wrapMode: Text.Wrap
                    }
                    Label {
                        objectName: entry.row.controlId.replace("starlight.", "warning.")
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: entry.row.warning
                        // System Settings' neutral tone: a consequence, not an error.
                        color: Kirigami.Theme.neutralTextColor
                        font: Kirigami.Theme.smallFont
                        wrapMode: Text.Wrap
                    }
                }
            }
            // A few options, all in view: the radio buttons of a System Settings
            // page. One Tab stop, with the arrow keys moving between them.
            Component {
                id: choiceComponent
                FocusScope {
                    id: choice
                    objectName: entry.row.controlId
                    enabled: entry.row.sensitive
                    implicitWidth: choiceColumn.implicitWidth
                    implicitHeight: choiceColumn.implicitHeight
                    readonly property int currentIndex: entry.row.currentIndex
                    readonly property var model: entry.row.options
                    readonly property string displayText: entry.row.displayText
                    function activated(index) { entry.row.activated(index) }
                    function moveChoice(index) {
                        if (index < 0 || index >= entry.row.options.length) return
                        entry.row.activated(index)
                    }
                    Accessible.role: Accessible.Grouping
                    Accessible.name: entry.row.accessibleName
                    Accessible.description: entry.row.accessibleDescription
                    ButtonGroup { id: choiceGroup }
                    ColumnLayout {
                        id: choiceColumn
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: entry.row.options
                            delegate: RadioButton {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                ButtonGroup.group: choiceGroup
                                text: entry.row.textRole.length > 0 ? modelData[entry.row.textRole] : modelData
                                checked: index === entry.row.currentIndex
                                focus: index === Math.max(0, entry.row.currentIndex)
                                activeFocusOnTab: index === Math.max(0, entry.row.currentIndex)
                                Accessible.name: text
                                Keys.onUpPressed: choice.moveChoice(index - 1)
                                Keys.onDownPressed: choice.moveChoice(index + 1)
                                onClicked: if (index !== entry.row.currentIndex) entry.row.activated(index)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            visible: text.length > 0
                            text: entry.row.subtitle
                            font: Kirigami.Theme.smallFont
                            opacity: 0.7
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
            Component {
                id: sliderComponent
                ColumnLayout {
                    spacing: 2
                    RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        PlasmaSlider {
                            id: bitrateSlider
                            Layout.fillWidth: true
                            objectName: entry.row.controlId
                            accessibleName: entry.row.accessibleName
                            accessibleDescription: entry.row.accessibleDescription
                            enabled: entry.row.sensitive
                            from: entry.row.from
                            to: entry.row.to
                            // Breeze draws a tick for every step, which is a solid bar at
                            // this many; snapping happens here instead.
                            stepSize: 0
                            function report(wanted) {
                                var step = entry.row.stepSize
                                entry.row.moved(Math.max(entry.row.from, Math.min(entry.row.to, Math.round(wanted / step) * step)))
                            }
                            Keys.onLeftPressed: report(value - (mirrored ? -2 : 2) * entry.row.stepSize)
                            Keys.onRightPressed: report(value + (mirrored ? -2 : 2) * entry.row.stepSize)
                            WheelHandler { onWheel: function(event) { bitrateSlider.report(bitrateSlider.value + (event.angleDelta.y > 0 ? 2 : -2) * entry.row.stepSize) } }
                            // Dragging self-writes value and would break a binding;
                            // follow the model imperatively, and only at rest.
                            Component.onCompleted: value = entry.row.value
                            Connections {
                                target: entry.row
                                function onValueChanged() { if (!bitrateSlider.pressed) bitrateSlider.value = entry.row.value }
                            }
                            onMoved: report(value)
                        }
                        Label { text: entry.row.valueText }
                    }
                    Label {
                        objectName: entry.row.controlId.replace("starlight.", "warning.")
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: entry.row.warning
                        // System Settings' neutral tone: a consequence, not an error.
                        color: Kirigami.Theme.neutralTextColor
                        font: Kirigami.Theme.smallFont
                        wrapMode: Text.Wrap
                    }
                }
            }
            Component {
                id: actionComponent
                ColumnLayout {
                    spacing: 2
                    PlasmaButton {
                        objectName: entry.row.controlId
                        text: entry.row.title
                        accessibleName: entry.row.accessibleName
                        accessibleDescription: entry.row.accessibleDescription
                        enabled: entry.row.sensitive
                        onClicked: entry.row.triggered()
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: entry.row.subtitle
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                        wrapMode: Text.Wrap
                    }
                }
            }
            // Progressive disclosure, as KDE's "Show Advanced Settings" toggles.
            Component {
                id: expanderComponent
                ColumnLayout {
                    spacing: 2
                    PlasmaButton {
                        objectName: entry.row.controlId
                        flat: true
                        text: entry.row.title
                        icon.name: entry.row.expanded ? "lucide-chevron-down" : (mirrored ? "lucide-chevron-left" : "lucide-chevron-right")
                        accessibleName: entry.row.accessibleName
                        accessibleDescription: entry.row.accessibleDescription
                        Accessible.checkable: true
                        Accessible.checked: entry.row.expanded
                        onClicked: entry.row.expanded = !entry.row.expanded
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        visible: text.length > 0
                        text: entry.row.subtitle
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
