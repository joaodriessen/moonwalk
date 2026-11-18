import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// One setting in a session surface: words on the left, its control on the
// right. Plain Controls, so each desktop's style paints it natively.
RowLayout {
    id: row
    property string title: ""
    property string subtitle: ""
    default property alias control: trailing.data
    spacing: 12
    Layout.fillWidth: true
    Layout.minimumHeight: 44

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: 2
        Label {
            id: titleLabel
            Layout.fillWidth: true
            visible: text.length > 0
            text: row.title
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: row.subtitle
            wrapMode: Text.Wrap
            opacity: 0.7
            font.pixelSize: Math.max(11, Math.round(titleLabel.font.pixelSize > 0
                                                    ? titleLabel.font.pixelSize * 0.86
                                                    : titleLabel.fontInfo.pixelSize * 0.86))
        }
    }
    RowLayout {
        id: trailing
        Layout.alignment: Qt.AlignVCenter
        spacing: 8
    }
}
