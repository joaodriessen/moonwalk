import QtQuick 2.9
import QtQuick.Controls 2.2

// The application has one terminal quit chain whose origin must stay alive
// until it either hands off to a stream or recovers.  Keep the native
// StackView private so every destructive mutation crosses this boundary.
FocusScope {
    id: root

    property var pinnedOrigin: null
    property var deferredMutations: []

    readonly property alias currentItem: nativeStack.currentItem
    readonly property alias depth: nativeStack.depth
    readonly property alias busy: nativeStack.busy

    property var pushEnter: null
    property var pushExit: null
    property var popEnter: null
    property var popExit: null

    function copyArguments(args) {
        var values = []
        for (var index = 0; index < args.length; ++index)
            values.push(args[index])
        return values
    }

    function runMutation(kind, values) {
        return nativeStack[kind].apply(nativeStack, values)
    }

    function enqueueOrRun(kind, args) {
        var values = copyArguments(args)
        if (pinnedOrigin !== null) {
            deferredMutations = deferredMutations.concat([({
                kind: kind,
                values: values
            })])
            return currentItem
        }
        return runMutation(kind, values)
    }

    function push() {
        // A pushed item would sit above QuitSegue and make its terminal pop or
        // replace target the wrong item. Unlike destructive caller intent,
        // there is no safe ordering to replay, so reject it while leased.
        if (pinnedOrigin !== null)
            return currentItem
        return nativeStack.push.apply(nativeStack, arguments)
    }

    function canAcceptQuitRoute(route) {
        return pinnedOrigin === null && route !== null
    }

    function canBeginQuitLease(route, segue) {
        return canAcceptQuitRoute(route) && segue !== null
    }

    // This is the only stack transition permitted to establish a quit lease.
    // Its route and segue are supplied together, so callers cannot first pin
    // an origin and then place an unrelated item above the terminal segue.
    function beginQuitLease(route, segue) {
        if (!canBeginQuitLease(route, segue))
            return null
        pinnedOrigin = route
        try {
            nativeStack.push(segue)
            // StackView may return the unchanged current item when a source
            // cannot be loaded. Only the requested segue becoming current is
            // a successful lease transition.
            if (nativeStack.currentItem === segue)
                return segue
        } catch (error) {
            // StackView rejects an invalid route synchronously. Fall through
            // to restore the boundary before the caller rejects its effect.
        }
        pinnedOrigin = null
        return null
    }

    function pop() {
        return enqueueOrRun("pop", arguments)
    }

    function replace() {
        return enqueueOrRun("replace", arguments)
    }

    function clear() {
        return enqueueOrRun("clear", arguments)
    }

    function get(index) {
        return nativeStack.get(index)
    }

    function find(callback, onlySearchLoadedItems) {
        return nativeStack.find(callback, onlySearchLoadedItems)
    }

    function unpinOrigin(route) {
        if (pinnedOrigin !== route)
            return
        pinnedOrigin = null
        Qt.callLater(flushDeferredMutations)
    }

    function flushDeferredMutations() {
        if (pinnedOrigin !== null || deferredMutations.length === 0)
            return
        var pending = deferredMutations
        deferredMutations = []
        for (var index = 0; index < pending.length; ++index)
            runMutation(pending[index].kind, pending[index].values)
    }

    StackView {
        id: nativeStack
        anchors.fill: parent
        pushEnter: root.pushEnter
        pushExit: root.pushExit
        popEnter: root.popEnter
        popExit: root.popExit
    }
}
