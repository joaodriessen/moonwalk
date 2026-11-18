.pragma library

// AbstractButton.click() was added in Qt 6.8. Keep the richer API when it is
// available, while preserving the same checked/clicked route on the Qt 6.4
// compatibility floor.
function activate(control) {
    if (!control || !control.enabled) {
        return false
    }

    if (typeof control.click === "function") {
        control.click()
        return true
    }

    if (control.checkable && typeof control.toggle === "function") {
        control.toggle()
    }
    if (typeof control.clicked === "function") {
        control.clicked()
        return true
    }

    return false
}
