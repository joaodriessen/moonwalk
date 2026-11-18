import QtQuick 2.15
import "../HostError.js" as HostError

// The host dialogs as data: which one is showing, what it says and what its
// responses do. Each desktop paints this in its own dialog anatomy.
QtObject {
    id: root
    required property var page
    // Called after any response so the surface can restore keyboard focus.
    signal responded()

    readonly property var effectiveOverlay: page.interruptOverlay && page.interruptOverlay.kind !== "none"
                                            ? page.interruptOverlay : page.baseOverlay
    readonly property string kind: effectiveOverlay ? effectiveOverlay.kind : "none"
    readonly property var payload: effectiveOverlay && effectiveOverlay.payload ? effectiveOverlay.payload : ({})
    // Kinds that only present information: a single Close response.
    readonly property bool readOnly: kind === "details" || kind === "connection-test"
                                     || kind === "error" || kind === "presets"
    readonly property bool destructive: kind === "confirm-end" || kind === "confirm-delete" || kind === "confirm-quit"
    readonly property bool hasInput: kind === "add-host" || kind === "rename-host"
                                     || kind === "custom-resolution" || kind === "custom-frame-rate"
    property string textValue: ""
    property string secondaryValue: ""

    readonly property string addHostError: kind === "add-host" && payload.error
                                           && textValue.trim() === String(payload.address || "")
                                           ? String(payload.error) : ""
    readonly property bool addHostBusy: kind === "add-host" && payload.dispatched === true

    // No bigger than this display shows; the page says how big that is.
    readonly property int maximumWidth: payload.maximumWidth || payload.maximum || 8192
    readonly property int maximumHeight: payload.maximumHeight || payload.maximum || 8192
    readonly property int maximumFrameRate: payload.maximum || 9999
    function validInteger(value, minimum, maximum) {
        return /^\d+$/.test(value.trim()) && Number(value) >= minimum && Number(value) <= maximum
    }
    readonly property bool canAccept: {
        switch (kind) {
        case "add-host": return textValue.trim().length > 0 && !addHostBusy
        case "rename-host": return textValue.trim().length > 0
        case "custom-resolution": return validInteger(textValue, 256, maximumWidth) && validInteger(secondaryValue, 256, maximumHeight)
        case "custom-frame-rate": return validInteger(textValue, 10, maximumFrameRate)
        default: return true
        }
    }
    // What a text field starts from when a dialog opens. Functions of the live
    // overlay, because they run inside the kind's own change signal, before
    // bindings that depend on it have necessarily updated.
    function inputKind() {
        var current = page.interruptOverlay && page.interruptOverlay.kind !== "none"
                ? page.interruptOverlay : page.baseOverlay
        return current || ({ kind: "none", payload: ({}) })
    }
    function hasInputNow() {
        return ["add-host", "rename-host", "custom-resolution", "custom-frame-rate"].indexOf(inputKind().kind) >= 0
    }
    function initialText() {
        var current = inputKind()
        var values = current.payload || ({})
        switch (current.kind) {
        case "add-host": return String(values.address || "")
        case "rename-host": return String(values.name || "")
        case "custom-resolution": return String(values.width || "")
        case "custom-frame-rate": return String(values.fps || "")
        default: return ""
        }
    }
    function initialSecondaryText() {
        var current = inputKind()
        return current.kind === "custom-resolution" ? String((current.payload || {}).height || "") : ""
    }

    readonly property string title: {
        switch (kind) {
        case "add-host": return qsTranslate("PcView", "Add Host")
        case "pairing": return payload.hostName ? qsTranslate("PcView", "Pair with %1").arg(payload.hostName)
                                                : qsTranslate("PcView", "Pair Host")
        case "connection-test": return qsTranslate("PcView", "Test Internet Streaming Ports")
        case "confirm-quit": return payload.retainedDesktopTakeover === true ? qsTranslate("PcView", "Take Over Desktop?") : qsTranslate("PcView", "Quit Running App?")
        case "confirm-end": return qsTranslate("PcView", "End Remote Session?")
        case "confirm-delete": return payload.name ? qsTranslate("PcView", "Remove %1?").arg(payload.name)
                                                   : qsTranslate("PcView", "Remove Host?")
        case "rename-host": return qsTranslate("PcView", "Rename Host")
        case "details": return qsTranslate("PcView", "Host Details")
        case "custom-resolution": return qsTranslate("PcView", "Custom Resolution")
        case "custom-frame-rate": return qsTranslate("PcView", "Custom Frame Rate")
        case "presets": return qsTranslate("PcView", "Saved Settings")
        case "error": return qsTranslate("PcView", "Connection Problem")
        default: return ""
        }
    }
    readonly property string semanticId: {
        switch (kind) {
        case "pairing": return "starlight.hosts.pairing"
        case "error": return "starlight.hosts.error"
        case "confirm-delete": return "starlight.hosts.confirm-delete"
        case "add-host": return "starlight.hosts.add-dialog"
        case "confirm-quit": return "starlight.hosts.confirm-quit"
        case "confirm-end": return "starlight.hosts.confirm-end"
        case "custom-resolution": return "starlight.hosts.custom-resolution"
        case "custom-frame-rate": return "starlight.hosts.custom-frame-rate"
        case "presets": return "starlight.hosts.presets"
        default: return ""
        }
    }
    readonly property string pin: String(payload.pin || "")
    readonly property string body: {
        switch (kind) {
        case "add-host": return qsTranslate("PcView", "The address of the computer running the streaming host.")
        case "pairing": return qsTranslate("PcView", "Enter this PIN on %1. Moonwalk Host asks for it on screen; on Sunshine and compatible hosts, open the web interface and choose PIN.")
                               .arg(payload.hostName || qsTranslate("PcView", "the host"))
        case "connection-test":
            if (payload.dispatched === true)
                return qsTranslate("PcView", "Moonwalk is testing your network connection to determine if any required ports are blocked.")
            if (payload.result === 0)
                return qsTranslate("PcView", "No Internet streaming ports were blocked by this network. This does not test your host or local network connection.")
            if (payload.result > 0)
                return qsTranslate("PcView", "Required Internet streaming ports are blocked by this network. Streaming over the Internet may not work here.") + "\n\n" + qsTranslate("PcView", "The following network ports were blocked:") + "\n" + (payload.blockedPorts || "")
            if (payload.result === -1)
                return qsTranslate("PcView", "The network test could not be performed because Moonwalk's connection-testing servers weren't reachable. Check your internet connection or try again later.")
            return qsTranslate("PcView", "Moonwalk is testing your network connection to determine if any required ports are blocked.")
        case "confirm-quit": return payload.retainedDesktopTakeover === true
            ? qsTranslate("PcView", "End the host’s retained Desktop stream and prepare a main display for this device? Other viewers of that stream will be disconnected.")
            : qsTranslate("PcView", "Quit %1 before connecting? Any unsaved progress in that app will be lost.").arg(payload.runningAppLabel || qsTranslate("PcView", "the running app"))
        case "confirm-end": return qsTranslate("PcView", "End %1 on the host without starting another stream? Unsaved work may be lost.").arg(payload.runningAppLabel || qsTranslate("PcView", "the remote session"))
        case "confirm-delete": return qsTranslate("PcView", "You will need to pair again to stream from this host.")
        case "details": return payload.details || ""
        case "error": return HostError.message(payload)
        default: return ""
        }
    }
    readonly property string primaryPlaceholder: kind === "add-host" ? qsTranslate("PcView", "Address or hostname")
                                                 : kind === "rename-host" ? qsTranslate("PcView", "Host name")
                                                 : kind === "custom-frame-rate" ? qsTranslate("PcView", "Frames per second")
                                                 : qsTranslate("PcView", "Width")
    readonly property string secondaryPlaceholder: qsTranslate("PcView", "Height")
    readonly property string rangeNote: kind === "custom-resolution"
                                        ? qsTranslate("PcView", "Up to %1×%2, the size of this display").arg(maximumWidth).arg(maximumHeight)
                                        : kind === "custom-frame-rate"
                                          ? qsTranslate("PcView", "10–%1 fps, as fast as this display refreshes").arg(maximumFrameRate) : ""
    readonly property string acceptLabel: {
        switch (kind) {
        case "confirm-delete": return qsTranslate("PcView", "Remove")
        case "confirm-quit": return payload.retainedDesktopTakeover === true ? qsTranslate("PcView", "Take Over and Connect") : qsTranslate("PcView", "Quit and Connect")
        case "confirm-end": return qsTranslate("PcView", "End Session")
        case "add-host": return qsTranslate("PcView", "Add")
        case "rename-host": return qsTranslate("PcView", "Rename")
        case "custom-resolution":
        case "custom-frame-rate": return qsTranslate("PcView", "Apply")
        default: return qsTranslate("PcView", "Continue")
        }
    }
    readonly property string rejectLabel: readOnly ? qsTranslate("PcView", "Close") : qsTranslate("PcView", "Cancel")

    // "Name: value" lines from the backend read better as a labelled list.
    readonly property var detailRows: {
        var rows = []
        var lines = String(payload.details || "").split("\n")
        for (var index = 0; index < lines.length; ++index) {
            var line = lines[index]
            var split = line.indexOf(": ")
            if (split > 0) rows.push({ label: line.substring(0, split), value: line.substring(split + 2) })
            else if (line.trim().length > 0) rows.push({ label: "", value: line })
        }
        return rows
    }

    function dismissCurrent() {
        if (effectiveOverlay && effectiveOverlay.operationId >= 0)
            page.dismissOverlay(effectiveOverlay.operationId)
    }
    function acceptCurrent() {
        if (!canAccept) return
        var operationId = effectiveOverlay.operationId
        switch (kind) {
        case "add-host": page.submitAddHost(textValue); break
        case "confirm-quit": page.confirmQuitAndConnect(operationId); break
        case "confirm-end": page.confirmEndSession(operationId); break
        case "confirm-delete": page.confirmDelete(operationId); break
        case "rename-host": page.submitRename(operationId, textValue); break
        case "custom-resolution": page.submitCustomResolution(operationId, Number(textValue), Number(secondaryValue)); break
        case "custom-frame-rate": page.submitCustomFrameRate(operationId, Number(textValue)); break
        case "details":
        case "connection-test":
        case "presets":
        case "error": dismissCurrent(); break
        default: return
        }
        responded()
    }
    function rejectCurrent() {
        if (kind === "pairing") page.cancelPairing(payload.attemptId)
        else if (kind === "confirm-quit") page.rejectQuit(effectiveOverlay.operationId)
        else dismissCurrent()
        responded()
    }

    // Saved settings, shared by both dialogs.
    property string presetStatus: ""
    function savePreset(name) {
        var normalized = String(name || "").trim()
        if (!normalized) return false
        var saved = page.savePreset(normalized)
        presetStatus = saved ? qsTranslate("PcView", "Saved “%1”.").arg(normalized)
                             : qsTranslate("PcView", "Could not save these settings.")
        return saved
    }
    function applyPreset(id, name) {
        presetStatus = page.applyPreset(id) ? qsTranslate("PcView", "Now using “%1”. Applies next connection.").arg(name)
                                            : qsTranslate("PcView", "Could not apply “%1”.").arg(name)
    }
    function duplicatePreset(id, name) {
        presetStatus = page.duplicatePreset(id, qsTranslate("PcView", "%1 copy").arg(name))
                       ? "" : qsTranslate("PcView", "Could not duplicate “%1”.").arg(name)
    }
    function renamePreset(id, name) {
        var normalized = String(name || "").trim()
        if (!normalized) return false
        var renamed = page.renamePreset(id, normalized)
        presetStatus = renamed ? "" : qsTranslate("PcView", "Could not rename this setup.")
        return renamed
    }
    function removePreset(id, name) {
        presetStatus = page.removePreset(id) ? qsTranslate("PcView", "Removed “%1”.").arg(name)
                                             : qsTranslate("PcView", "Could not remove “%1”.").arg(name)
    }
}
