# Changelog

All notable changes to Moonwalk are documented here, in the spirit of
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The `starlight-v0.3.0-alpha` tag marks a July 2026 private-alpha source snapshot;
it did not have a published installer or GitHub Release. Older `v0.0.3`…`v6.1.0`
tags are inherited from upstream moonlight-qt, not Starlight releases. Changes
since that snapshot remain grouped under the current private tester version.

## 0.3.1-alpha.17 (private tester build, 2026-11-16)

- Automatic Display always enables the standard host display-mode request, even when an obsolete hidden game-optimization preference was disabled. GeForce Experience retains its supported-resolution guard.
- Preserve explicit command-line settings after applying Automatic session defaults, including frame rate and display preparation; safe retries keep their bounded settings.
- Show the existing stock-host virtual-display-disabled warning through stream configuration warnings without reconnecting or changing the request.
- The alpha.16 custom-host protocol experiment was withdrawn. This build uses existing paired host features; Windows display preparation on some hosts remain under investigation and are not claimed fixed by this release.

## 0.3.1-alpha.15 (private tester build, 2026-11-15)

- Performance tuning never disconnects an established stream automatically. Picture reductions, detail recovery and undo of unsuccessful trials are deferred to the next normal connection.
- Offer a quiet Apply Now action in Stream Controls, explaining that it briefly reconnects. Real connection-failure recovery preserves the current picture instead of applying a pending experiment.
- Scope deferred recommendations and independent trial baselines to host, build, route, viewing display and codec; expire them after one day and discard ineffective trial learning without interrupting video.
- Apply automatic bitrate targets only over a live control contract or a previously measured live path. Unknown/rebuilding HTTP paths learn next-connection targets without disruptive probes; show the applied bitrate separately from deferred estimates.

## 0.3.1-alpha.14 (private tester build, 2026-11-15)

- Detect a Desktop retained by another client on hosts advertising session-role controls; offer an explicit takeover instead of silently joining its old display.
- Use the host's advertised Terminate control with fresh paired-state/catalogue checks, bounded confirmation, and post-termination verification before a new Desktop launch. No host update or service restart is part of this flow.
- Verify caller ownership after launch on hosts with projected session roles; stop rather than open an unowned shared display.
- Separate new main-Desktop performance observations from historical shared-capture learning; retain transport/encoder-change information.
- Preserve the standard protocol path for hosts without these optional controls. Stock takeover remains non-atomic and requires approval; main-display geometry, windows, scaling and restore remain field qualification.

## 0.3.1-alpha.13 (private tester build, 2026-11-13)

- Connect targets the existing main Desktop, rather than automatically creating a separate Remote Monitor. Release this client's retained auxiliary monitor before resolving the normal Desktop.
- Explicitly close HTTP connections between paired host requests to avoid reusing a connection whose host-side client-certificate identity is unavailable.
- Alpha.12 achieved native capture on an additional monitor but did not preserve the user's existing desktop/windows; it is superseded for normal Connect behavior. Actual main-desktop handoff and restoration remain subject to field qualification and host display policy.

## 0.3.1-alpha.12 (private tester build, 2026-11-12)

- Use an advertised private Remote Monitor role for Automatic virtual desktops, with role-specific activation/reconnect and verified client-scoped cleanup; preserve standard Desktop behavior on other hosts and explicit physical/host choices.
- Resolve GUI Desktop connections from the authenticated current catalogue, including retained-private-display transitions to physical/host mode.
- Send application UUIDs exactly as advertised to compatible hosts, alongside numeric IDs.
- Reject malformed application catalogues during role checks and cleanup verification.
- Alpha.11's UUID handling did not resolve retained Desktop capture sizing; this release addresses the separate private-role lifecycle. Native capture and broad host-version coverage still require field verification.

## 0.3.1-alpha.11 (private tester build, 2026-11-12)

- Preserve host application UUIDs and distinguish the real Desktop target from Vibepollo's duplicate running-app shortcut, including titles padded for sorting.
- Apply Desktop input/display/cleanup policies consistently to padded host titles.

## 0.3.1-alpha.10 (private tester build, 2026-11-12)

- Refresh guide screenshots from current layouts with synthetic data; report failed preview scenes as failures.
- Clarify that Desktop cleanup runs in the background without promising it can always succeed.
- Refuse Flatpak staging paths that could overwrite source, provenance, exports or builder state; reject tracked private files in staged revisions.
- Tighten release-pin checks for ambiguous manifests, untracked build inputs and installed license drift.
- Include source license notices in Linux installations and document remaining dependency/artwork review.
- Replace the diagnostic dialog's unconditional secrecy promise with a reminder to review before sharing.
- Add security-report guidance and useful version fields to support forms.
- Hide IPv6 addresses, URL credentials, Windows home paths, quoted/OAuth secrets and truncated PEM blocks in copied diagnostic summaries.
- Reject common accidentally tracked credential files and private agent state in repository checks.
- Clarify platform/host compatibility, installation, security reporting and release requirements.
- Check documentation links and product-version consistency in CI; harden workflow permissions and dependency tracking.

## 0.3.1-alpha.9 (private tester build, 2026-11-06)

- Carry automatic tuning trials, previous working modes and costly encoder-change observations across reconnects.
- Stop ignored or ineffective size trials, retain a reduced bitrate ceiling, and bound tuning interruptions with one rollback.
- Preserve the host desktop geometry during encoded-size tuning when the host cannot negotiate UI scale.
- Require authenticated paired-client state before target actions and when verifying remote cleanup; authenticate fresh TLS connections explicitly.

Native-resolution smoothness on constrained hosts and universal initial Windows UI scaling remain unverified. This is not a guarantee of native-resolution playback on every host.

## 0.3.1-alpha.8 (private tester build, 2026-11-06)

- Return to the launcher on disconnect while remote cleanup continues in the background; surface cleanup failures afterward.
- Try bitrate adjustments before gradual size reductions, preserve requested frame rate, and restore a better prior mode when a trial does not help.

## 0.3.1-alpha.7 (private tester build, 2026-11-05)

- Improve Desktop target cleanup and connection recovery.
- Qualify automatic learning against activity and delivered cadence instead of treating processing delay alone as a capacity limit.
- Follow the local light/dark theme in the HUD; improve HUD rendering, text sizing and cursor coordinate handling.
- Show host software without its version on the main page, simplify host decoration, and add Re-evaluate This Host.

## 0.3.1-alpha.6, native interface (private tester build, 2026-11-01)

- Starlight is now called Moonwalk, and its Mac host, Skylight, is now Moonwalk Host. Settings, pairings, the app ID and the `starlight` command are unchanged.
- GNOME: a libadwaita-faithful split view with app and host menus, one flowing host page with Quality first, status pages for hosts that need something, and Preferences as a dialog.
- KDE Plasma: a Kirigami application with one toolbar, a view-coloured host list, Host Actions beside Connect, System Settings style forms and inline messages.
- Hyprland and other desktops: the Adwaita-derived surface without fake window controls, following portal colours, fonts and the compositor's window size, including tiling. A Quit menu item appears where a window may have no close button.
- Every surface renders one shared declaration of settings and dialogs, so capabilities stay equal; host limits stay visible with their reason.
- Stream controls, connecting, failure recovery and quitting are native on each desktop, and no longer name a Mac on hosts that are not.
- Saved Settings and Restore Last Working Settings are reachable again; pairing confirms success and offers to connect; the last-used host is reselected on launch.
- Host pages open with a card showing who the host is, whether it can be reached, the one thing to do next and what the connection will be, with Automatic resolved to its real value and when it last connected. Each host has its own coloured tile.
- Options few enough to show together are an AdwToggleGroup on GNOME and radio buttons on Plasma. Automatic pointer and absolute mouse are one Pointer choice. Long per-row explanations are cut to a line.
- Preferences can be searched (Ctrl+F). A running app or a wake in progress is a banner with its action beside it.
- Pairing shows the PIN as separate digits; Host Details can copy each value; shortcuts draw their keys; dialogs are bottom sheets on narrow GNOME windows; About can copy diagnostics.
- Connecting shows the host and counted stages in plain words; failure names the host. Stream Controls shows live round trip, frame rate and connection state, and sets bitrate with a slider in Mbps.
- Toasts appear again: since the first native redesign they had no parent and were never drawn. The application icon's corners are rounded wherever it is drawn by Qt.
- Command-line launches use the native style and status pages. Host pages mirror for right-to-left languages; pages move with short transitions unless the desktop asks for less motion.

## 0.3.1-alpha.6 (private candidate, 2026-10-28)

- Simplify the computer page around Connect, availability and named Display, Quality, Keyboard and Audio settings.
- Add discoverable in-stream controls, explicit Release Input and independent Fit in Window / Actual Size viewing.
- Keep candidate connection settings provisional until sustained healthy presentation; retain the previous working configuration for recovery.
- Queue multiple regular files to a named destination, with per-file progress, cancellation, safe collision choices and Reveal for received files.
- Bound decoder readiness and explain probe failure without starting a competing SDL owner.
- Distinguish resumable targets, active streaming, capture readiness and viewing-only permission state when the host provides those facts.

This remains a private candidate. Live acceptance evidence and remaining hardware limits are recorded separately.

## 0.3.1-alpha.5 (private candidate, 2026-10-22)

- Re-evaluate automatic connection routes for each session; add deliberate per-host address selection.
- Reconnect interrupted sessions with bounded retries and Cancel, preserving the target and current stream controls.
- Offer temporary safe settings, explicit Keep after video appears, last-successful restore and named host presets.
- Resume or end the actual running app, and wait for a woken host to become ready before connecting.
- Add live stream controls, optional local Compose/IME typing, desktop scale and opt-in adaptive bitrate choices.
- Negotiate PNG clipboard exchange and bidirectional regular-file transfer with progress, cancellation, checksum verification and destination conflict checks.

New host capabilities require a compatible Skylight version. See [while you're streaming](docs/guides/while-streaming.md) for controls and limits. This is a private candidate, not a public release.

## 0.3.0-alpha (private tester build)

Everything in this release is drawn from the fork's commit history, from the moonlight-qt
merge-base (`c0c4d605`) to the current tip — short hashes are cited so each item can be traced
back to its commit.

### Design system & HIG UI

- Laid the Starlight design-system foundation: `Theme.qml` design tokens plus the first `SL*`
  Qt Quick primitives (`SLTextField`, `SLSlider`, `SLBusy`) (`7484becf`, `ffef5028`).
- Replaced the upstream app-grid with a host-first interface — a sidebar listing hosts only, one
  page per host, no separate app-level Settings tab (`93c9a2f2`, `28aa2698`).
- Consolidated the main page into a single flagship host card, then gave it a hero treatment: a
  subtle lit-from-top gradient and an unpaired-state invite line (`440ebf9d`, `ecf46612`,
  `75f4d912`).
- Rebuilt `SettingsView` in the HIG language with zero feature loss, then made it adaptive —
  essentials up front, everything else one click away (`2541c1e3`, `4d569cc5`).
- Polished the interface against Apple's HIG across multiple waves: dialogs/menus/segues,
  keyboard-only focus rings, theme-aware icon tinting, macOS-style scrollbars, inline rename,
  geometry continuity, Tab-focus autoscroll, native macOS-style menus
  (`a177a264`, `86c974dc`, `1def6814`, `2320e20b`, `1bde4cee`, `a55b6cd6`, `88bfb2d7`,
  `02e130e0`, `4f8ad729`, `4f216289`, `42d1b064`, `1977fd26`).
- Followed the desktop's live light/dark color scheme via `Qt.styleHints.colorScheme`, with a
  luma-based fallback for older Qt/no-portal sessions (`f871d3e3`).
- Five owner-ruling polish pass on the host card and its defaults (`8102d6c1`).
- New "Liquid Glass" app icon (`a72688c9`).

### Flatpak packaging

- Added Flatpak packaging as the primary distribution channel; dropped AppImage (`8237eb3e`).
  The manifest bundles a patched FFmpeg with NVIDIA NVDEC and VAAPI/VDPAU/Vulkan hardware
  acceleration, plus the legacy `i965` VAAPI driver for pre-Broadwell Intel iGPUs, so hardware
  decode works without host-side driver setup.
- Bundled `QAdwaitaDecorations` for a native GNOME titlebar with a real drop shadow, matching
  KDE's out-of-the-box Breeze titlebar (`1b15ae1e`, `d81a6afc`).
- Fixed VAAPI hardware decode on Wayland after `vaGetDisplayWl` stopped working (`14243f1a`).
- Fixed a Flatpak-specific stale QML disk-cache bug (reproducible-build timestamp
  normalization defeating Qt's cache invalidation) via `QML_DISABLE_DISK_CACHE=1` plus
  AOT-compiled QML (`f871d3e3`).
- Corrected the README's account of GNOME decoration-plugin availability (`3e87afc6`).

### Skylight coordination

- Coordinated client-side cursor drawing with the host via the `clientSideCursor` launch
  parameter, with Skylight-aware mouse defaults (`94d57afd`).
- Send the real hostname when pairing, replacing the legacy hardcoded `"roth"` devicename
  sentinel — backward-compatible with hosts that don't read it (`5494eb64`).
- Default new hosts to native display resolution, capped to 1080p on decode hardware that
  lacks HEVC/AV1 acceleration (`a78d388a`).
- Documented the settings-ownership contract between Starlight (client) and Skylight (host):
  client owns preference, host owns capability/policy (`5cbb586b`, `7f5e0ed1`).
- Extended the default-bitrate table past 4K for 5K/6K clients (`da800e4e`).
- Per-host connection settings, keyed by host rather than global (`66ded382`).

### Pairing & config identity

- Starlight owns its own configuration identity, with a one-time, copy-only import from an
  existing Moonlight install — the original Moonlight configuration is never modified
  (`9723b8e7`).
- Rebranded packaging, icon, titlebar, and app name from Moonlight to Starlight, preserving the
  config path during the transition (`e62a866a`, `dafecb9d`).
- Hardened client pairing: no silent no-op, no crash on certificate failure (`ab2ca17f`).

### Fixes & hardening

- Keep brightness and volume keys on the local Linux machine while streaming,
  including the SDL Wayland scancode mapping needed for brightness keys
  (`677f4adf`).
- Fixed `--help` still invoking the binary as `moonlight` (`0a1196d5`).
- Migrated legacy vsync/pacing settings and made preset detection honest about what it detects
  (`88a99e2f`).
- Probe the union of stored and freshly-resolved host addresses at connect time (`3539abe9`).
- Cut pre-stream connect delay by decoupling the toast gate and trimming probe attempts
  (`b72571c3`).
- Fixed the launcher opening too tall on Wayland and fixed invisible wake feedback
  (`46a57420`).
- Fixed dialog/popup overflow, right-click handling, window sizing, and geometry logging
  (`cfcbf1d4`).
- Compacted the default window size and its persistence, fixed the Wayland app-id, renamed a
  stray label (`8735da95`).
- Release-build undefined-behavior guards, plus a broader production-readiness hardening pass
  covering crash-safety, correctness, and UX (`fa0bc0f3`, `3c7cfc60`).
- Clipboard sync (an inherited upstream feature, not Starlight-original): moved to a dedicated
  thread with event-driven long-poll (`23c8e5b4`, `aa2b044b`).
- Fresh-user polish: real clone URL in docs, de-duplicated README headings, KDE detection
  (`2e66b7a1`).

### Docs & foundation

- Recorded the phase-1 fork doctrine: pristine engine, the UI is the fork, no invented settings
  (see `docs/design/2026-07-13-phase1.md`).
- Landed the initial Starlight UI on the pristine moonlight-qt engine (`ffef5028`), carrying
  forward the two pre-fork wrapper-era patches as first-class code (`929c4f59`).
- Alpha M2: public-facing README pass, version set to 0.3.0, AppStream metadata reality-checked
  against what actually ships (`27fcb380`).
