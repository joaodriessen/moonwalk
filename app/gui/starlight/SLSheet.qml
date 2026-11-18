import QtQuick 2.9
import Starlight 1.0

// Shared HIG "sheet" chrome for modal popups: rounded card, hairline border,
// opaque solid fill. Matches the mockup's .sheet style (docs/design/2026-07-13
// -hig-mockup.html) and the hand-rolled pairing sheet in PcView.qml, now
// formalized into one component.
//
// Usage: set as a Popup/Dialog's `background:` delegate (see
// NavigableDialog.qml). The dimmed backdrop itself is a separate concern --
// set via that Popup's `Overlay.modal` using Theme.sheetDim.
//
// `shadow` paints a soft drop shadow around the sheet from a pre-rendered
// 9-patch (res/sheet_shadow.png) -- deliberately NOT a MultiEffect/shader
// shadow, which renders nothing on non-GPU paths (the same failure class
// that made the old runtime-colorized icons invisible). Popups don't clip
// their background, so the halo can extend past the sheet's bounds.
Rectangle {
    id: root

    property bool shadow: false

    color: Theme.cardSolid
    radius: Theme.radiusCard
    border.width: 1
    border.color: Theme.hairline

    BorderImage {
        visible: root.shadow
        z: -1
        anchors.fill: parent
        // Extend beyond the sheet, biased downward like a light-from-above
        // shadow; the sheet itself covers the sharp core of the 9-patch.
        anchors.leftMargin: -22
        anchors.rightMargin: -22
        anchors.topMargin: -18
        anchors.bottomMargin: -26
        source: "qrc:/res/sheet_shadow.png"
        border { left: 42; right: 42; top: 42; bottom: 42 }
        // The shadow reads heavier against light content; keep it subtler there.
        opacity: Theme.isDark ? 0.9 : 0.55
    }
}
