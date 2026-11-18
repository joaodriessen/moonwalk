import QtQuick 2.9
import QtQuick.Layouts 1.3
import Starlight 1.0

// One grouped-form row: label + optional sub-label on the left, arbitrary
// trailing control (toggle/combo/slider/text chip) on the right, per the
// mockup's .row / .rowlbl / .rowsub.
//
// SLGroup stacks these in a Column; since Column has no per-child index
// attached property, the *caller* sets topBorder: false on the first row
// of each group (mirrors the mockup's ".row + .row" all-but-first rule).
Item {
    id: root

    property string label: ""
    property string sublabel: ""
    property bool topBorder: true
    // The usual form row has one trailing control. Complex rows opt in with
    // accessibleControl explicitly, so the visible label never guesses which
    // of several controls it names.
    property Item accessibleControl: trailing.children.length === 1 ? trailing.children[0] : null
    default property alias trailingData: trailing.data

    function applyAccessibleLabel() {
        var control = root.accessibleControl
        if (!control || root.label.length === 0) {
            return
        }

        // Some composite controls (currently SLTextField and SLDisclosure)
        // expose their real focusable semantic Item through accessibleItem.
        // Associate the visible label with that Item, rather than an outer
        // visual wrapper which assistive technology does not focus.
        var semanticControl = control
        if (control.hasOwnProperty("accessibleItem") && control.accessibleItem) {
            semanticControl = control.accessibleItem
        }

        // Set labelledBy rather than labelFor. Qt 6.10 introduced these
        // relations, and this direction avoids the direct labelFor binding
        // issue tracked upstream while still creating both sides of the
        // relationship.
        if ("labelledBy" in semanticControl.Accessible) {
            semanticControl.Accessible["labelledBy"] = labelText
        }

        // Public semantic inputs take precedence. Empty component defaults
        // inherit the durable form label/sub-label rather than a painted chip
        // or icon, keeping semantics independent of the visual adapter.
        if (control.hasOwnProperty("accessibleName")
                && (!control.accessibleName || control.accessibleName.length === 0)) {
            control.accessibleName = root.label
        }
        if (root.sublabel.length > 0
                && control.hasOwnProperty("accessibleDescription")
                && (!control.accessibleDescription || control.accessibleDescription.length === 0)) {
            control.accessibleDescription = root.sublabel
        }
    }

    onAccessibleControlChanged: applyAccessibleLabel()
    onLabelChanged: applyAccessibleLabel()
    onSublabelChanged: applyAccessibleLabel()
    Component.onCompleted: applyAccessibleLabel()

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(44, mainRow.implicitHeight + 22)

    Rectangle {
        visible: root.topBorder
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.hairline
    }

    RowLayout {
        id: mainRow
        anchors.fill: parent
        anchors.leftMargin: 15
        anchors.rightMargin: 15
        anchors.topMargin: 11
        anchors.bottomMargin: 11
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            Text {
                id: labelText
                Layout.fillWidth: true
                text: root.label
                color: Theme.label
                font.pixelSize: Theme.sizeBody
                wrapMode: Text.Wrap

                Accessible.role: Accessible.StaticText
                Accessible.name: root.label
                Accessible.description: root.sublabel
                Accessible.ignored: root.label.length === 0
            }

            Text {
                Layout.fillWidth: true
                text: root.sublabel
                visible: root.sublabel.length > 0
                color: Theme.label2
                font.pixelSize: Theme.sizeFootnote
                lineHeight: 1.3
                wrapMode: Text.Wrap
            }
        }

        Item {
            id: trailing
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
