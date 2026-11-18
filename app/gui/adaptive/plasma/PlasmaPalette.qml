// No placeholderText role here: the QML Palette only gained it in
// Qt 6.5, and this file must load on the supported Qt 6.4 floor.
import QtQuick 2.15
import QtQuick.Controls 2.15
import Starlight 1.0

Control {
    id: root
    visible: false

    SystemPalette { id: systemActive; colorGroup: SystemPalette.Active }
    SystemPalette { id: systemInactive; colorGroup: SystemPalette.Inactive }
    SystemPalette { id: systemDisabled; colorGroup: SystemPalette.Disabled }

    palette.active.window: PlasmaAppearance.ready ? PlasmaAppearance.active.window : systemActive.window
    palette.active.windowText: PlasmaAppearance.ready ? PlasmaAppearance.active.windowText : systemActive.windowText
    palette.active.base: PlasmaAppearance.ready ? PlasmaAppearance.active.base : systemActive.base
    palette.active.alternateBase: PlasmaAppearance.ready ? PlasmaAppearance.active.alternateBase : systemActive.alternateBase
    palette.active.text: PlasmaAppearance.ready ? PlasmaAppearance.active.text : systemActive.text
    palette.active.button: PlasmaAppearance.ready ? PlasmaAppearance.active.button : systemActive.button
    palette.active.buttonText: PlasmaAppearance.ready ? PlasmaAppearance.active.buttonText : systemActive.buttonText
    palette.active.highlight: PlasmaAppearance.ready ? PlasmaAppearance.active.highlight : systemActive.highlight
    palette.active.highlightedText: PlasmaAppearance.ready ? PlasmaAppearance.active.highlightedText : systemActive.highlightedText
    palette.active.toolTipBase: PlasmaAppearance.ready ? PlasmaAppearance.active.toolTipBase : systemActive.toolTipBase
    palette.active.toolTipText: PlasmaAppearance.ready ? PlasmaAppearance.active.toolTipText : systemActive.toolTipText
    palette.active.light: PlasmaAppearance.ready ? PlasmaAppearance.active.light : systemActive.light
    palette.active.midlight: PlasmaAppearance.ready ? PlasmaAppearance.active.midlight : systemActive.midlight
    palette.active.mid: PlasmaAppearance.ready ? PlasmaAppearance.active.mid : systemActive.mid
    palette.active.dark: PlasmaAppearance.ready ? PlasmaAppearance.active.dark : systemActive.dark
    palette.active.shadow: PlasmaAppearance.ready ? PlasmaAppearance.active.shadow : systemActive.shadow

    palette.inactive.window: PlasmaAppearance.ready ? PlasmaAppearance.inactive.window : systemInactive.window
    palette.inactive.windowText: PlasmaAppearance.ready ? PlasmaAppearance.inactive.windowText : systemInactive.windowText
    palette.inactive.base: PlasmaAppearance.ready ? PlasmaAppearance.inactive.base : systemInactive.base
    palette.inactive.alternateBase: PlasmaAppearance.ready ? PlasmaAppearance.inactive.alternateBase : systemInactive.alternateBase
    palette.inactive.text: PlasmaAppearance.ready ? PlasmaAppearance.inactive.text : systemInactive.text
    palette.inactive.button: PlasmaAppearance.ready ? PlasmaAppearance.inactive.button : systemInactive.button
    palette.inactive.buttonText: PlasmaAppearance.ready ? PlasmaAppearance.inactive.buttonText : systemInactive.buttonText
    palette.inactive.highlight: PlasmaAppearance.ready ? PlasmaAppearance.inactive.highlight : systemInactive.highlight
    palette.inactive.highlightedText: PlasmaAppearance.ready ? PlasmaAppearance.inactive.highlightedText : systemInactive.highlightedText
    palette.inactive.toolTipBase: PlasmaAppearance.ready ? PlasmaAppearance.inactive.toolTipBase : systemInactive.toolTipBase
    palette.inactive.toolTipText: PlasmaAppearance.ready ? PlasmaAppearance.inactive.toolTipText : systemInactive.toolTipText
    palette.inactive.light: PlasmaAppearance.ready ? PlasmaAppearance.inactive.light : systemInactive.light
    palette.inactive.midlight: PlasmaAppearance.ready ? PlasmaAppearance.inactive.midlight : systemInactive.midlight
    palette.inactive.mid: PlasmaAppearance.ready ? PlasmaAppearance.inactive.mid : systemInactive.mid
    palette.inactive.dark: PlasmaAppearance.ready ? PlasmaAppearance.inactive.dark : systemInactive.dark
    palette.inactive.shadow: PlasmaAppearance.ready ? PlasmaAppearance.inactive.shadow : systemInactive.shadow

    palette.disabled.window: PlasmaAppearance.ready ? PlasmaAppearance.disabled.window : systemDisabled.window
    palette.disabled.windowText: PlasmaAppearance.ready ? PlasmaAppearance.disabled.windowText : systemDisabled.windowText
    palette.disabled.base: PlasmaAppearance.ready ? PlasmaAppearance.disabled.base : systemDisabled.base
    palette.disabled.alternateBase: PlasmaAppearance.ready ? PlasmaAppearance.disabled.alternateBase : systemDisabled.alternateBase
    palette.disabled.text: PlasmaAppearance.ready ? PlasmaAppearance.disabled.text : systemDisabled.text
    palette.disabled.button: PlasmaAppearance.ready ? PlasmaAppearance.disabled.button : systemDisabled.button
    palette.disabled.buttonText: PlasmaAppearance.ready ? PlasmaAppearance.disabled.buttonText : systemDisabled.buttonText
    palette.disabled.highlight: PlasmaAppearance.ready ? PlasmaAppearance.disabled.highlight : systemDisabled.highlight
    palette.disabled.highlightedText: PlasmaAppearance.ready ? PlasmaAppearance.disabled.highlightedText : systemDisabled.highlightedText
    palette.disabled.toolTipBase: PlasmaAppearance.ready ? PlasmaAppearance.disabled.toolTipBase : systemDisabled.toolTipBase
    palette.disabled.toolTipText: PlasmaAppearance.ready ? PlasmaAppearance.disabled.toolTipText : systemDisabled.toolTipText
    palette.disabled.light: PlasmaAppearance.ready ? PlasmaAppearance.disabled.light : systemDisabled.light
    palette.disabled.midlight: PlasmaAppearance.ready ? PlasmaAppearance.disabled.midlight : systemDisabled.midlight
    palette.disabled.mid: PlasmaAppearance.ready ? PlasmaAppearance.disabled.mid : systemDisabled.mid
    palette.disabled.dark: PlasmaAppearance.ready ? PlasmaAppearance.disabled.dark : systemDisabled.dark
    palette.disabled.shadow: PlasmaAppearance.ready ? PlasmaAppearance.disabled.shadow : systemDisabled.shadow
}
