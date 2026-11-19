# Accessibility checks for Hosts and Preferences

Run these steps in a logged-in graphical session with the Moonwalk build
you're evaluating, Orca, and an AT-SPI inspector if available. Offscreen and
synthetic controller tests help catch routing bugs. Check speech and the
accessibility tree separately in the desktop session. If an accessibility
tool is unavailable, record the gap.

## Setup

1. Use a non-production test host if a real connection or pairing request is
   needed. Keep one known host available, and have one deliberately invalid
   address available for the error path.
2. Start Orca and confirm it can read another Qt application. If Qt does not
   expose accessibility automatically in this desktop session, launch the
   test instance once with `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1`; do not make
   that setting persistent.
3. Start Moonwalk normally, with no `offscreen` platform override. Open the
   AT-SPI tree inspector alongside it when available.
4. Record the Moonwalk commit, Qt version, desktop/session type, Orca
   version, and whether an inspector was available.

## Hosts landmark and selection

1. Move Orca focus into the main Hosts page. Verify it announces a pane named
   **Hosts** and, in the inspector, that the page has identifier
   `starlight.hosts.page`.
2. Navigate to the hosts list. Verify it is announced as a list named
   **Available streaming hosts** on GNOME or **Hosts** on Plasma, with
   identifier `starlight.hosts.list`.
3. Move between at least two host entries using the existing keyboard route.
   Each entry must announce its host name, online/checking state, and selected
   state; selection must change the visible host card without starting a
   stream.
4. With an entry selected, activate its existing connect/wake route once only
   when it is safe to do so. Confirm the screen-reader action has the same
   result as ordinary keyboard activation.
5. Open **Main Menu** on GNOME or Plasma and activate its settings item.
   In the fallback view, activate **More settings**. Confirm that the action
   opens the same Settings view as `Ctrl+,`.

## Pairing and recovery

1. Return to Hosts. Tab to **Add Host** on GNOME or Plasma, or **Pair a new
   host** in the fallback view. Check the visible focus indicator, open the
   form with Return or controller confirm, then close it. Reopen it with
   its assistive action. On GNOME and Plasma the dialog has identifier
   `starlight.hosts.add-dialog` and the field is named **Address or hostname**.
2. Enter a synthetic invalid address. Confirm the error text is readable,
   the address remains available to edit, and **Cancel** returns to Hosts.
3. Select an unpaired test host. Activate **Pair** and check that the PIN
   dialog is named **Pair with _host_**, with instructions to enter the PIN
   on the host. Its identifier is `starlight.hosts.pairing` on GNOME and Plasma
   and `starlight.hosts.pairing-dialog` in the fallback view.
4. Activate **Cancel** through the assistive action. Verify that the dialog
   closes and the host can be selected again. Separately check that focus
   returns to the host workflow and keyboard or controller navigation
   remains available.

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
4. In Plasma, press Tab from **Back to hosts**. Focus should move to
   **Search preferences**, then to **Streaming display mode** on the next
   Tab. In GNOME, check that you can move from **Close Preferences** to
   **Streaming display mode** and that keyboard focus stays in the dialog.
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
