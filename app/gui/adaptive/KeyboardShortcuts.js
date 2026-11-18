function releaseHint() {
    return qsTranslate("KeyboardShortcuts", "Ctrl+Alt+Shift+O opens stream controls; Ctrl+Alt+Shift+Z releases or recaptures input; Ctrl+Alt+Shift+Q disconnects.")
}
function reference() {
    return qsTranslate("KeyboardShortcuts", "During a stream, hold Ctrl+Alt+Shift and press:") + "\n\n"
        + qsTranslate("KeyboardShortcuts", "Q — Disconnect from the stream") + "\n"
        + qsTranslate("KeyboardShortcuts", "O — Open stream controls") + "\n"
        + qsTranslate("KeyboardShortcuts", "Z — Release or recapture keyboard and mouse input") + "\n"
        + qsTranslate("KeyboardShortcuts", "X — Toggle fullscreen") + "\n"
        + qsTranslate("KeyboardShortcuts", "K — Toggle capture of system keyboard shortcuts") + "\n"
        + qsTranslate("KeyboardShortcuts", "S — Toggle performance statistics") + "\n"
        + qsTranslate("KeyboardShortcuts", "V — Type clipboard text on the host") + "\n\n"
        + qsTranslate("KeyboardShortcuts", "Typing clipboard text is a fallback when automatic text clipboard synchronization is unavailable. Input release and fullscreen require a desktop environment.")
}

// The same reference as data, for surfaces that draw the keys.
function groups() {
    const held = ["Ctrl", "Alt", "Shift"]
    function during(key, label) { return { keys: held.concat([key]), label: label } }
    return [
        { title: qsTranslate("KeyboardShortcuts", "During a Stream"),
          note: qsTranslate("KeyboardShortcuts", "Typing clipboard text is a fallback when automatic text clipboard synchronization is unavailable."),
          shortcuts: [
              during("O", qsTranslate("KeyboardShortcuts", "Open stream controls")),
              during("Q", qsTranslate("KeyboardShortcuts", "Disconnect from the stream")),
              during("Z", qsTranslate("KeyboardShortcuts", "Release or recapture input")),
              during("X", qsTranslate("KeyboardShortcuts", "Toggle fullscreen")),
              during("K", qsTranslate("KeyboardShortcuts", "Toggle system shortcut capture")),
              during("S", qsTranslate("KeyboardShortcuts", "Toggle performance statistics")),
              during("V", qsTranslate("KeyboardShortcuts", "Type clipboard text on the host"))
          ] },
        { title: qsTranslate("KeyboardShortcuts", "In Moonwalk"),
          shortcuts: [
              { keys: ["Ctrl", "N"], label: qsTranslate("KeyboardShortcuts", "Add a host") },
              { keys: ["Ctrl", ","], label: qsTranslate("KeyboardShortcuts", "Preferences") },
              { keys: ["Ctrl", "?"], label: qsTranslate("KeyboardShortcuts", "Keyboard shortcuts") },
              { keys: ["F10"], label: qsTranslate("KeyboardShortcuts", "Main menu") },
              { keys: ["Enter"], label: qsTranslate("KeyboardShortcuts", "Connect to the selected host") },
              { keys: ["Ctrl", "Q"], label: qsTranslate("KeyboardShortcuts", "Quit") }
          ] }
    ]
}
