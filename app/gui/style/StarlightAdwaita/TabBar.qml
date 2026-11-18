import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import "../../adaptive/gnome" as G

// AdwInlineViewSwitcher: a rounded trough holding equal-width toggles.
T.TabBar {
    id: control
    readonly property G.GnomeTokens tokens: G.GnomeTokens {}
    implicitWidth: Math.max(implicitBackgroundWidth, contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight, contentHeight + topPadding + bottomPadding)
    padding: 3
    spacing: 3
    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        snapMode: ListView.SnapToItem
        highlightMoveDuration: 0
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: 40
        preferredHighlightEnd: width - 40
    }
    background: Rectangle {
        radius: control.tokens.radiusCard
        color: control.tokens.control
    }
}
