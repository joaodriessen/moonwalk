# Live accessibility validation: Hosts and Settings

This is the release gate for the semantic-control work. Run it in a normal
logged-in graphical session with the production-built Starlight binary. Do
not use `QT_QPA_PLATFORM=offscreen`, the QML test runner, or synthetic
controller-key tests as a substitute: those checks only prove QML routing,
not the AT-SPI tree or what Orca announces.

No package installation is required for this check. If Orca or an AT-SPI tree
inspector such as Accerciser is unavailable, record that as an environment
blocker rather than changing the machine.

## Setup

1. Use a non-production test host if a real connection or pairing request is
   needed. Keep one known host available, and have one deliberately invalid
   address available for the error path.
2. Start Orca and confirm it can read another Qt application. If Qt does not
   expose accessibility automatically in this desktop session, launch the
   test instance once with `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`; do not make
   that setting persistent.
3. Start Starlight normally, with no `offscreen` platform override. Open the
   AT-SPI tree inspector alongside it when available.
4. Record the Starlight commit, Qt version, desktop/session type, Orca
   version, and whether an inspector was available.

## Hosts landmark and selection

1. Move Orca focus into the main Hosts page. Verify it announces a pane named
   **Hosts** and, in the inspector, that the page has identifier
   `starlight.hosts.page`.
2. Navigate to the hosts list. Verify it is announced as a list named
   **Hosts** with identifier `starlight.hosts.list`.
3. Move between at least two host entries using the existing keyboard route.
   Each entry must announce its host name, online/checking state, and selected
   state; selection must change the visible host card without starting a
   stream.
4. With an entry selected, activate its existing connect/wake route once only
   when it is safe to do so. Confirm the screen-reader action has the same
   result as ordinary keyboard activation.
5. Inspect the settings gear: it must announce **More settings**, describe
   that it opens global settings, and activate through the normal route.

## Pairing and recovery

1. Return to Hosts. Tab to **Pair a new host** and verify the visible focus
   treatment appears; activate it once with Return or the established
   controller-confirm route, then close it. Open it once more through its
   assistive action. Each route must use the ordinary pairing dialog. Enter
   the test host address only; do not use a personal or production target.
2. When the PIN sheet appears, verify a dialog is announced as **Pair with
   _host_**. In the inspector, its semantic content has identifier
   `starlight.hosts.pairing-dialog` and a description explaining that the PIN
   is entered on the host.
3. Verify the Cancel button announces **Cancel**, describes that it cancels
   the current attempt, and activates exactly the ordinary cancel path.
4. Cancel the pairing. Confirm focus recovers to the host workflow and the
   app remains keyboard/controller navigable.
5. Exercise one safe error path (for example, an invalid test address or a
   deliberately failed test pairing). Confirm the error dialog is announced,
   its visible error/help text is readable, its available actions work, and
   closing it returns usable focus rather than trapping or losing it.

## Settings landmark, labels, states, and focus

1. Open global settings. Verify a pane named **Settings** with identifier
   `starlight.settings.page`.
2. Inspect or navigate to **Streaming display mode**. It must be a combo box
   with identifier `starlight.settings.window-mode`, a visible-label
   relationship to **Streaming display mode**, and an announcement that does
   not collapse to only the currently selected option. Open and close it with
   its normal assistive action; do not commit an unwanted preference change.
3. Verify **Language** is similarly labelled and reachable, with identifier
   `starlight.settings.language`.
4. Tab from **Back to hosts** once. Focus must move to Streaming display mode,
   preserving the established keyboard and controller order.
5. Navigate to **Advanced**. It must be announced as a toggle button with
   checked/unchecked state matching whether the disclosure is open. Invoke it
   twice and verify both the visible panel and focusable descendants appear
   and disappear together.
6. Check a representative toggle such as **Mute the host's speakers while
   streaming**. It must expose checkbox role, checked state, a concise name,
   and the same change route for its press/toggle action as pointer and
   keyboard input. Restore the prior value afterward.
7. Check **Capture system keyboard shortcuts** and its adjacent mode control.
   Verify labels describe the two distinct controls and that disabled state
   remains truthful when the platform does not support capture. An assistive
   action on a disabled Back, settings, toggle, dropdown, disclosure, slider,
   or scroll bar must do nothing—matching pointer and keyboard behavior.

## Pass/fail record

Pass only when every named control is discoverable, its spoken role/name/state
matches the visible UI, actions follow the ordinary interaction path, and
focus recovers after cancel/error flows. Record failures with the exact spoken
text, AT-SPI role/state/action list, screenshot or short recording when
possible, and the setup details above.

The automated `tst_accessibility.qml` contract is complementary evidence only:
it checks semantic properties and action routing in real QML roots, but cannot
certify an Orca announcement or an AT-SPI backend tree.
