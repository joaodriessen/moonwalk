import QtQuick 2.9
import QtQuick.Controls 2.2
import Starlight 1.0

// Preserve the selected style's indicator implementation and animation while
// applying Starlight's semantic accent token when that delegate exposes a
// color property. Native styles such as org.kde.desktop use a plain Item here;
// capability-checking preserves their active user-style implementation.
BusyIndicator {
    id: control

    Binding {
        target: control.contentItem
        property: "color"
        value: Theme.accent
        when: control.contentItem !== null
              && "color" in control.contentItem
    }
}
