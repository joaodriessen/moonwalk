import QtQuick 2.15
import QtTest 1.3
import "TestCompat.js" as TestCompat

TestCase {
    name: "PageContract"

    readonly property var requiredKeys: [
        "id", "surfacePredicate", "contextState", "command",
        "uuidRequired", "validation", "confirmation", "persistence",
        "saveCardinality", "dependentEffects", "overlay", "focusReturnId",
        "recovery"
    ]

    readonly property var hostIds: [
        "hosts.page", "hosts.display-target", "hosts.pointer-mode",
        "hosts.connection-address", "hosts.desktop-scale", "hosts.view-scale",
        "hosts.local-text-input", "hosts.resume-running", "hosts.end-session",
        "hosts.list", "hosts.item.<uuid>", "hosts.empty-state", "hosts.add-host",
        "hosts.setup-guide",
        "hosts.pair", "hosts.wake", "hosts.connection-test", "hosts.connect",
        "hosts.unsupported-server",
        "hosts.confirm-quit", "hosts.rename", "hosts.delete", "hosts.details",
        "hosts.custom-resolution", "hosts.custom-frame-rate", "hosts.open-settings",
        "hosts.about",
        "hosts.stream-mode", "hosts.display-mode", "hosts.input-mode",
        "hosts.resolution", "hosts.frame-rate",
        "hosts.video-codec", "hosts.limit-bandwidth", "hosts.maximum-bitrate",
        "hosts.local-cursor",
        "hosts.saved-settings", "hosts.restore-last-working", "hosts.relearn-performance",
        "hosts.preset-name", "hosts.preset-save"
    ]

    readonly property var settingsIds: [
        "settings.page", "settings.search", "settings.window-mode", "settings.mute-host-speakers", "settings.language",
        "settings.mute-on-focus-loss", "settings.quit-app-after",
        "settings.ui-display-mode", "settings.connection-warnings",
        "settings.configuration-warnings", "settings.rich-presence",
        "settings.keep-awake", "settings.capture-system-keys-mode",
        "settings.capture-system-keys", "settings.virtual-trackpad",
        "settings.swap-mouse-buttons", "settings.reverse-scroll",
        "settings.swap-face-buttons", "settings.single-controller",
        "settings.gamepad-mouse", "settings.background-gamepad", "settings.mdns",
        "settings.detect-network-blocking", "settings.performance-overlay"
    ]

    function sorted(values) {
        return values.slice().sort()
    }

    function test_contract_rows_are_complete_and_exact() {
        const contract = TestCompat.readJson("qrc:/tests/data/page-contract-v1.json")
        compare(contract.version, 1)
        verify(contract.rows instanceof Array)

        contract.rows.forEach(function(row) {
            requiredKeys.forEach(function(key) {
                verify(key in row, row.id + " misses " + key)
            })
        })

        compare(sorted(contract.rows.map(function(row) { return row.id })),
                sorted(hostIds.concat(settingsIds)))
    }

    function test_graph_walk_is_repeatable_for_anonymous_and_duplicate_nodes() {
        const firstDuplicate = { objectName: "duplicate" }
        const secondDuplicate = { objectName: "duplicate" }
        const firstAnonymous = { objectName: "" }
        const secondAnonymous = { objectName: "" }
        const dataDuplicate = { objectName: "duplicate" }
        const dataAnonymous = { objectName: "" }
        const alphaProperty = { objectName: "" }
        const zetaProperty = { objectName: "duplicate" }
        const root = {
            objectName: "root",
            children: [firstDuplicate, secondDuplicate, firstAnonymous, secondAnonymous],
            data: [dataDuplicate, dataAnonymous],
            zeta: zetaProperty,
            alpha: alphaProperty
        }
        const expected = [root, firstDuplicate, secondDuplicate,
                          firstAnonymous, secondAnonymous, dataDuplicate,
                          dataAnonymous, alphaProperty, zetaProperty]

        for (let run = 0; run < 3; ++run) {
            const actual = TestCompat.walkQObjectGraph(root)
            compare(actual.length, expected.length)
            for (let index = 0; index < expected.length; ++index) {
                verify(actual[index] === expected[index],
                       "run " + run + ", discovery index " + index)
            }
        }
    }
    function test_graph_walk_deduplicates_cycles_without_merging_equal_nodes() {
        var first = { objectName: "same" }
        var second = { objectName: "same" }
        var root = { objectName: "root", children: [first, second], data: [first, second] }
        first.owner = root
        second.peer = first
        var actual = TestCompat.walkQObjectGraph(root)
        compare(actual.length, 3)
        verify(actual[0] === root)
        verify(actual[1] === first)
        verify(actual[2] === second)
    }

    function test_graph_walk_does_not_expand_value_objects_or_facade_snapshots() {
        var child = { objectName: "child" }
        var root = { objectName: "root", children: [child],
                     snapshot: { nested: { objectName: "not-a-child" } },
                     palette: { window: "#ffffff" } }
        var actual = TestCompat.walkQObjectGraph(root)
        compare(actual.length, 2)
        verify(actual[0] === root)
        verify(actual[1] === child)
    }

}
