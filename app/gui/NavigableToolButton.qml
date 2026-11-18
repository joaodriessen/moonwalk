import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import Starlight 1.0

ToolButton {
    property string iconSource

    activeFocusOnTab: true
    hoverEnabled: true

    icon.source: iconSource
    icon.width: background.width
    icon.height: background.height

    // Glyphs are recoloured to icon.color, so one source serves both
    // palettes (e.g. PcView's sidebar cog). Default to white so the legacy blue ToolBar in main.qml -- which has
    // its own colored background and isn't Theme-aware -- keeps its exact
    // existing look. HIG surfaces override this per-instance, e.g.
    // `icon.color: Theme.label`.
    icon.color: "white"

    // Flat rounded-rect hover/press (Theme.segBg/track), matching the
    // sidebar row / footer hover language, instead of the Material ripple
    // -- the one icon affordance in the HIG surfaces that wasn't flat.
    background: Rectangle {
        // anchors.fill (not implicit sizing) -- icon.width/height above derive
        // from background.width/height, so this must keep filling the control's
        // actual size the way the Material default background did.
        anchors.fill: parent
        radius: Theme.radiusChip
        color: parent.pressed ? Theme.track : (parent.hovered ? Theme.segBg : "transparent")
        border.width: parent.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    // This determines the size of the Material highlight. We increase it
    // from the default because we use larger than normal icons for TV readability.
    Layout.preferredHeight: parent.height

    Keys.onReturnPressed: {
        clicked()
    }

    Keys.onEnterPressed: {
        clicked()
    }

    Keys.onRightPressed: {
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
    }

    Keys.onLeftPressed: {
        nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
    }
}
