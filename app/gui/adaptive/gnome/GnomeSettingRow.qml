import QtQuick 2.15

// Paints one shared SettingRow as the matching Adwaita row. Everything it
// knows comes from the row, so the three kinds below stay one-line mappings.
Loader {
    id: root
    required property var row
    // Inside an expander, every row follows the header.
    property bool alwaysSeparated: false
    readonly property bool separated: alwaysSeparated || y > 0.5
    readonly property bool shown: row.shown === true && row.filteredOut !== true
    // Loaded controls keep their objectNames even while a host hides the row;
    // visibility alone removes it from layout, Tab order and the screen reader.
    visible: shown
    width: parent ? parent.width : implicitWidth
    sourceComponent: row.kind === "combo" ? comboComponent
                     : row.kind === "choice" ? choiceComponent
                     : row.kind === "slider" ? sliderComponent
                     : row.kind === "action" ? actionComponent
                     : row.kind === "expander" ? expanderComponent
                     : switchComponent

    Component {
        id: switchComponent
        GnomeSwitchRow {
            title: root.row.title
            subtitle: root.row.subtitle
            switchObjectName: root.row.controlId
            warning: root.row.warning
            warningObjectName: root.row.controlId.replace("starlight.", "warning.")
            accessibleName: root.row.accessibleName
            accessibleDescription: root.row.accessibleDescription
            switchEnabled: root.row.sensitive
            checked: root.row.checked
            showSeparator: root.separated
            onToggled: root.row.toggled(toggleItem.checked)
        }
    }
    Component {
        id: comboComponent
        GnomeComboRow {
            title: root.row.title
            subtitle: root.row.subtitle
            comboObjectName: root.row.controlId
            warning: root.row.warning
            warningObjectName: root.row.controlId.replace("starlight.", "warning.")
            accessibleName: root.row.accessibleName
            accessibleDescription: root.row.accessibleDescription
            comboEnabled: root.row.sensitive
            model: root.row.options
            textRole: root.row.textRole
            currentIndex: root.row.currentIndex
            displayText: root.row.displayText.length > 0 ? root.row.displayText : comboItem.currentText
            showSeparator: root.separated
            onActivated: function(index) { root.row.activated(index) }
        }
    }
    Component {
        id: choiceComponent
        GnomeChoiceRow {
            setting: root.row
            showSeparator: root.separated
        }
    }
    Component {
        id: sliderComponent
        GnomeSliderRow {
            title: root.row.title
            sliderObjectName: root.row.controlId
            warning: root.row.warning
            warningObjectName: root.row.controlId.replace("starlight.", "warning.")
            accessibleName: root.row.accessibleName
            accessibleDescription: root.row.accessibleDescription
            sliderEnabled: root.row.sensitive
            from: root.row.from
            to: root.row.to
            stepSize: root.row.stepSize
            boundValue: root.row.value
            valueText: root.row.valueText
            showSeparator: root.separated
            onMoved: root.row.moved(sliderItem.value)
        }
    }
    Component {
        id: actionComponent
        GnomeActionRow {
            objectName: root.row.controlId
            title: root.row.title
            subtitle: root.row.subtitle
            accessibleName: root.row.accessibleName
            accessibleDescription: root.row.accessibleDescription
            destructive: root.row.destructive
            enabled: root.row.sensitive
            showChevron: true
            showSeparator: root.separated
            onClicked: root.row.triggered()
        }
    }
    Component {
        id: expanderComponent
        GnomeExpanderRow {
            row: root.row
            showSeparator: root.separated
        }
    }
}
