import QtQuick 2.15
import QtQuick.Window 2.15

// Keep keyboard and assistive focus visible without moving focus itself.
Item {
    id: root
    required property Flickable viewport
    readonly property Item focusedControl: trackedFocus()
    property real margin: 12
    width: 0
    height: 0

    function trackedFocus() {
        var focusWindow = viewport.Window.window
        var item = focusWindow ? focusWindow.activeFocusItem : null
        var ancestor = item
        while (ancestor && ancestor !== viewport.contentItem)
            ancestor = ancestor.parent
        return ancestor ? item : null
    }
    onFocusedControlChanged: Qt.callLater(revealFocus)
    function revealFocus() {
        var item = focusedControl
        if (!item || !item.visible || !viewport.visible) return
        var point = item.mapToItem(viewport.contentItem, 0, 0)
        var top = point.y - margin
        var bottom = point.y + item.height + margin
        var next = viewport.contentY
        if (top < next) next = top
        else if (bottom > next + viewport.height)
            next = item.height + 2 * margin <= viewport.height
                   ? bottom - viewport.height : top
        viewport.contentY = Math.max(0, Math.min(next,
                                      viewport.contentHeight - viewport.height))
    }
    Connections {
        target: root.focusedControl
        function onYChanged() { Qt.callLater(root.revealFocus) }
        function onHeightChanged() { Qt.callLater(root.revealFocus) }
    }
    Connections {
        target: root.viewport
        // Layout polish may place the focused row after its focus event was
        // handled (expansion, wrapping or a font change). Reveal again once
        // the content geometry is published, without waiting for new focus.
        function onContentHeightChanged() { Qt.callLater(root.revealFocus) }
        function onHeightChanged() { Qt.callLater(root.revealFocus) }
        function onWidthChanged() { Qt.callLater(root.revealFocus) }
    }
}
