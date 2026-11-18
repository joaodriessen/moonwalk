import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3
import Starlight 1.0
import "../../app/gui/adaptive/gnome" as Product

TestCase {
    id: testCase
    name: "GnomeHeaderIcons"
    when: windowShown
    visible: true
    width: 200
    height: 100

    Component {
        id: sceneComponent
        Rectangle {
            width: 180
            height: 90
            color: "#202020"
            ToolButton {
                objectName: "theme-calibration"
                x: 20; y: 20; width: 34; height: 34
                icon.name: "open-menu-symbolic"
                icon.width: 16; icon.height: 16
                icon.color: "magenta"
                display: AbstractButton.IconOnly
                background: Rectangle { color: "#202020" }
            }
            Product.GnomeHeaderButton {
                objectName: "header"
                x: 80; y: 20
                symbol: "open-menu"
            }
        }
    }

    function test_menuGlyphKeepsTransparentGapsUnderOpaqueTheme() {
        Appearance.reset()
        Appearance.colorScheme = 2
        var scene = createTemporaryObject(sceneComponent, testCase)
        verify(scene !== null)
        var calibration = findChild(scene, "theme-calibration")
        var header = findChild(scene, "header")
        verify(waitForRendering(scene))
        var actual = grabImage(scene)
        var scale = actual.width / scene.width
        var background = actual.pixel(0, 0)
        verify(background.r < 0.3, "Scene must be rendered on its dark background")
        var themed = actual.pixel(Math.floor((calibration.x + 17) * scale),
                                  Math.floor((calibration.y + 15) * scale))
        verify(themed.r > 0.9 && themed.b > 0.9 && themed.g < 0.1,
               "Fixture must reproduce the opaque themed icon")
        // Lucide's three bars sit at 5, 12 and 19 on its 24 grid; at 16 pixels
        // the middle one straddles two rows, so it reads a little softer.
        for (var y of [12, 17, 21])
            verify(actual.pixel(Math.floor((header.x + 17) * scale),
                                Math.floor((header.y + y) * scale)).r > 0.6,
                   "Menu stroke missing at " + y)
        for (var gap of [14, 19])
            verify(actual.pixel(Math.floor((header.x + 17) * scale),
                                Math.floor((header.y + gap) * scale)).r < 0.3,
                   "Menu gap filled by opaque theme artwork at " + gap)
    }
}
