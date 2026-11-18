import QtQuick 2.15

GnomeHeaderButton {
    id: root

    symbol: "previous"

    signal navigationRequested()

    onClicked: navigationRequested()
}
