import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3
import SdlGamepadKeyNavigation 1.0
import Starlight 1.0
import Starlight.Test 1.0
import SystemProperties 1.0

import "TestCompat.js" as TestCompat

TestCase {
    id: testCase
    name: "PageRoutes"
    when: windowShown

    property var hostRouteComponent: null
    property var settingsRouteComponent: null
    property var gnomeTokensComponent: null
    property var stack: null

    Component {
        id: testHostSourceComponent
        TestHostSource { }
    }

    function contextCount(route, objectName) {
        // Count this route's owned visual subtree. References such as Window.window
        // lead outside it and can reach other routes still alive in the StackView.
        var count = route && route.objectName === objectName ? 1 : 0
        var children = route && route.children ? route.children : []
        for (var index = 0; index < children.length; ++index)
            count += contextCount(children[index], objectName)
        return count
    }

    function objectByName(root, objectName) {
        return findChild(root, objectName)
    }

    function initTestCase() {
        hostRouteComponent = Qt.createComponent(Qt.resolvedUrl(
                                                   "../../app/gui/adaptive/HostPageRoute.qml"))
        verify(hostRouteComponent.status === Component.Ready,
               hostRouteComponent.errorString())

        settingsRouteComponent = Qt.createComponent(Qt.resolvedUrl(
                                                       "../../app/gui/adaptive/SettingsPageRoute.qml"))
        verify(settingsRouteComponent.status === Component.Ready,
               settingsRouteComponent.errorString())

        gnomeTokensComponent = Qt.createComponent(Qt.resolvedUrl(
                                                     "../../app/gui/adaptive/gnome/GnomeTokens.qml"))
        verify(gnomeTokensComponent.status === Component.Ready,
               gnomeTokensComponent.errorString())

        stack = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15;'
                    + ' StackView { width: 1000; height: 800 }',
                    testCase,
                    "page-route-stack")
        verify(stack !== null)
    }

    function cleanupTestCase() {
        if (stack) {
            stack.destroy()
            stack = null
        }
    }

    function test_hostRouteOwnsOneContextAndStableIdentity() {
        const route = stack.push(hostRouteComponent,
                                 { hostSource: testHostSourceComponent.createObject(testCase) })
        verify(route !== null, hostRouteComponent.errorString())
        compare(route.shellRouteKind, "hosts")
        compare(contextCount(route, "starlight.hosts.context"), 1)
        tryCompare(route, "shellRouteActive", true)
        stack.pop()
    }

    function test_settingsRouteOwnsOneContextAndStableIdentity() {
        const route = stack.push(settingsRouteComponent)
        verify(route !== null, settingsRouteComponent.errorString())
        compare(route.shellRouteKind, "settings")
        compare(contextCount(route, "starlight.settings.context"), 1)
        tryCompare(route, "shellRouteActive", true)
        stack.pop()
    }

    function test_reactivationForwardsEachContextAndSurfaceExactlyOncePerTransition() {
        const hosts = stack.push(hostRouteComponent,
                                 { hostSource: testHostSourceComponent.createObject(testCase) })
        tryCompare(hosts, "shellRouteActive", true)
        verify(hosts.routeLifecycleActive === undefined)
        verify(hosts.surfaceActivatedForCurrentRoute === undefined)
        compare(hosts.contextActivationTransitionCount, 1)
        compare(hosts.contextDeactivationTransitionCount, 0)
        compare(hosts.surfaceActivationForwardCount, 1)
        compare(hosts.surfaceDeactivationTransitionCount, 0)

        const settings = stack.push(settingsRouteComponent)
        verify(hosts !== null && settings !== null)
        verify(settings.routeLifecycleActive === undefined)
        verify(settings.surfaceActivatedForCurrentRoute === undefined)
        compare(hosts.context.routeActive, false)
        tryCompare(settings.context, "routeActive", true)
        compare(hosts.contextActivationTransitionCount, 1)
        compare(hosts.contextDeactivationTransitionCount, 1)
        compare(hosts.surfaceActivationForwardCount, 1)
        compare(hosts.surfaceDeactivationTransitionCount, 1)
        compare(settings.contextActivationForwardCount, 1)
        compare(settings.contextDeactivationForwardCount, 0)
        compare(settings.surfaceActivationForwardCount, 1)
        compare(settings.surfaceDeactivationTransitionCount, 0)

        var settingsDeactivation = null
        settings.context.routeActiveChanged.connect(function() {
            if (!settings.context.routeActive) {
                settingsDeactivation = ({
                                              contextActivations: settings.contextActivationForwardCount,
                                              contextDeactivations: settings.contextDeactivationForwardCount,
                                              surfaceActivations: settings.surfaceActivationForwardCount,
                                              surfaceDeactivations: settings.surfaceDeactivationTransitionCount
                                          })
            }
        })
        stack.pop()
        tryCompare(hosts.context, "routeActive", true)
        verify(settingsDeactivation !== null)
        compare(settingsDeactivation.contextActivations, 1)
        compare(settingsDeactivation.contextDeactivations, 1)
        compare(settingsDeactivation.surfaceActivations, 1)
        compare(settingsDeactivation.surfaceDeactivations, 1)
        compare(hosts.contextActivationTransitionCount, 2)
        compare(hosts.contextDeactivationTransitionCount, 1)
        compare(hosts.surfaceActivationForwardCount, 2)
        compare(hosts.surfaceDeactivationTransitionCount, 1)
        compare(contextCount(hosts, "starlight.hosts.context"), 1)
    }

    function test_appearanceProviderUpdateKeepsTheActiveRouteContextAndUuid() {
        Appearance.reset()
        var tokens = gnomeTokensComponent.createObject(testCase)
        verify(tokens !== null, gnomeTokensComponent.errorString())
        compare(tokens.dark, false)
        const source = testHostSourceComponent.createObject(testCase)
        source.replaceHosts([{ uuid: "a", name: "Alpha" }])
        const route = stack.push(hostRouteComponent, { hostSource: source })
        verify(route !== null, hostRouteComponent.errorString())
        tryCompare(route, "shellRouteActive", true)
        const before = route
        const contextBefore = route.context
        route.page.selectHostByUuid("a")
        const selectedBefore = route.page.selectedHostUuid

        Appearance.colorScheme = 2
        tryCompare(tokens, "dark", true)

        compare(route, before, "an appearance update must not reconstruct the route")
        compare(route.context, contextBefore,
                "an appearance update must retain the context owner")
        compare(route.page.selectedHostUuid, selectedBefore)
        compare(contextCount(route, "starlight.hosts.context"), 1)
        stack.pop()
        tokens.destroy()
        Appearance.reset()
    }

    function test_settingsRouteFocusesOnlyForGamepadsAndUsesTheVisibleControl() {
        var originalDesktopEnvironment = SystemProperties.hasDesktopEnvironment
        SdlGamepadKeyNavigation.reset()
        SystemProperties.hasDesktopEnvironment = true

        var focusWindow = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15; import QtQuick.Window 2.15;'
                    + ' Window { visible: true; width: 1000; height: 800;'
                    + ' property alias routeStack: routeStack;'
                    + ' StackView { id: routeStack; anchors.fill: parent } }',
                    testCase, "settings-route-focus-window")
        verify(focusWindow !== null)
        var focusStack = focusWindow.routeStack
        tryVerify(function() { return focusWindow.visible })

        var noGamepad = focusStack.push(settingsRouteComponent)
        tryCompare(noGamepad, "shellRouteActive", true)
        var windowMode = objectByName(noGamepad, "starlight.settings.window-mode")
        var language = objectByName(noGamepad, "starlight.settings.language")
        verify(windowMode !== null && language !== null)
        verify(windowMode.visible)
        verify(!noGamepad.context.shouldFocusInitialControl())
        verify(!windowMode.focus && !language.focus,
               "zero gamepads must not request either settings control focus")
        focusStack.pop()

        SdlGamepadKeyNavigation.connectedGamepads = 1
        var desktopSettings = focusStack.push(settingsRouteComponent)
        tryCompare(desktopSettings, "shellRouteActive", true)
        windowMode = objectByName(desktopSettings, "starlight.settings.window-mode")
        language = objectByName(desktopSettings, "starlight.settings.language")
        verify(windowMode.visible)
        verify(desktopSettings.context.shouldFocusInitialControl())
        tryVerify(function() { return windowMode.focus }, 5000,
                  "a connected gamepad must focus the visible window-mode control")
        verify(!language.focus)
        focusStack.pop()

        SystemProperties.hasDesktopEnvironment = false
        var nonDesktopSettings = focusStack.push(settingsRouteComponent)
        tryCompare(nonDesktopSettings, "shellRouteActive", true)
        windowMode = objectByName(nonDesktopSettings, "starlight.settings.window-mode")
        language = objectByName(nonDesktopSettings, "starlight.settings.language")
        verify(!windowMode.visible)
        verify(language.visible)
        tryVerify(function() { return language.focus }, 5000,
                  "without a visible window-mode row, gamepad focus must fall back to language")
        focusStack.pop()

        SdlGamepadKeyNavigation.reset()
        SystemProperties.hasDesktopEnvironment = originalDesktopEnvironment
        focusWindow.close()
        focusWindow.destroy()
    }
}
