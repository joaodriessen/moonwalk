// One translated recovery message for native and fallback host surfaces.
function message(payload) {
    payload = payload || {}
    switch (payload.code) {
    case "server-unsupported":
        return qsTranslate("PcView", "This build of Moonwalk cannot connect to the version of GeForce Experience on %1. Check for an update to Moonwalk.").arg(payload.hostName || "")
    case "desktop-target-unavailable":
        return qsTranslate("PcView", "%1 doesn't offer a Desktop entry. Add a desktop capture entry named Desktop in the host's application list, then connect again.").arg(payload.hostName || "")
    case "add-host-blocked":
        return qsTranslate("PcView", "Couldn't reach the host at that address.") + "\n\n"
             + qsTranslate("PcView", "The port check found blocked internet streaming ports. Check the host address and local connection separately.")
    case "add-host-unavailable":
        return qsTranslate("PcView", "Couldn't reach the host at that address.")
    default:
        return payload.message || qsTranslate("PcView", "The operation could not be completed.")
    }
}
