import QtQml 2.15

// One user-facing setting, declared once and painted natively by each desktop.
// Parity follows from this: every surface renders the same rows, with the same
// copy, gating and commands. `kind` is switch, combo, choice, slider, action or
// expander. A choice is a combo with few enough options to show them all at once.
QtObject {
    id: row
    default property list<QtObject> rows
    property string controlId: ""
    property string kind: "switch"
    property string title: ""
    property string subtitle: ""
    // What this choice is expected to cost, in plain words, shown beside the
    // control and read with it. Empty when the choice is fine.
    property string warning: ""
    property string accessibleName: title
    property string accessibleDescription: subtitle
    // Whether the row exists for this host, and whether it can be changed now.
    // A limitation of the host stays visible with its reason, not silently absent.
    property bool shown: true
    property bool sensitive: true
    property bool destructive: false
    // Set by a search over the settings: hides the row without changing `shown`.
    property bool filteredOut: false
    // Other words a person might search for.
    property string keywords: ""

    property bool checked: false

    property var options: []
    property string textRole: ""
    property int currentIndex: 0
    property string displayText: ""

    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 0
    property string valueText: ""

    property bool expanded: false

    signal toggled(bool wanted)
    signal activated(int index)
    signal moved(real wanted)
    signal triggered()
}
