// One translated recovery message for native and fallback host surfaces.
function message(payload) {
    payload = payload || {}
    switch (payload.code) {
    case "server-unsupported":
        return qsTranslate("PcView", "The version of GeForce Experience on %1 is not supported by this build of Moonwalk. You must update Moonwalk to stream from %1.").arg(payload.hostName || "")
    case "desktop-target-unavailable":
        return qsTranslate("PcView", "%1 doesn't expose a stream target named \"Desktop\". Add or rename a desktop capture target to \"Desktop\" in the host server, then reconnect.").arg(payload.hostName || "")
    case "add-host-blocked":
        return qsTranslate("PcView", "Unable to connect to the specified host.") + "\n\n"
             + qsTranslate("PcView", "This network is blocking Moonwalk. Streaming over the Internet may not work while connected to this network.")
    case "add-host-unavailable":
        return qsTranslate("PcView", "Unable to connect to the specified host.")
    default:
        return payload.message || qsTranslate("PcView", "The operation could not be completed.")
    }
}
