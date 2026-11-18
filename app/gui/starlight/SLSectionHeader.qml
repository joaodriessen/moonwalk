import QtQuick 2.9
import Starlight 1.0

// Small caps-weight section label above a group, per the mockup's
// .sectionlbl (see PcView.qml's inline "Quality" / "Video" / "Feel" /
// "Advanced" headers -- this just makes that pattern reusable so
// SettingsView's many sections don't hand-roll the same three properties
// seven times over).
Text {
    color: Theme.label2
    font.pixelSize: Theme.sizeCaption
    font.weight: Font.DemiBold
}
