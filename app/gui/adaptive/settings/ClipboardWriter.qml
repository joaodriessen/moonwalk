import QtQuick 2.15

// QML has no clipboard type of its own; an unseen text buffer has its copy().
TextEdit {
    visible: false
    function write(value) {
        text = value
        selectAll()
        copy()
        text = ""
    }
}
