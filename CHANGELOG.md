# Changelog

These notes cover Moonwalk and the earlier private Starlight builds. The
separate engine version in `app/version.txt` belongs to Moonlight Qt.

## Unreleased

- Pin every Flatpak Git dependency to a commit, keep local build paths out of
  staged provenance, and check the exported bundle against the source revision.
  Include the package's actual sandbox permissions in the verification report.
- Rewrite the README, guides, package description, and support forms to
  match the current client and the features compatible hosts provide.
- Clarify what the host page plans to request and what Moonwalk can confirm
  during a stream. Explain display, bitrate, pairing, and cleanup failures
  where they occur.
- Accept valid host application lists on Qt 6.4.2. The previous parser could
  reject a valid response at the end of the XML document.
- Leave pairing and session parameters out of request logs and diagnostic
  summaries. Logs still show the request endpoint and failure status.
- Keep the Linux settings file readable and writable only by its owner,
  including when importing a Moonlight profile.
- Remove the unused update checker inherited from Moonlight Qt. The About
  page links to Moonwalk's releases.
- Update the SDL libraries in the Flatpak for upstream Wayland clipboard,
  X11 cursor, Vulkan, and controller fixes.
- Include libdrm’s copyright and license notices in the Flatpak. Make source
  archives without the local paths in Flatpak Builder’s Git metadata.
- Follow the local light or dark theme even when the stream controls are
  collapsed to the handle. Keep the handle visible when frame pacing
  rebuilds the renderer mid-stream.
- Check host status when the launcher opens in the background, so a visible
  host list can update before it gets focus.
- Give the host a moment to release the stream during background disconnect
  cleanup. Leave sessions with other active viewers alone.
- Keep IPv6 addresses out of diagnostic summaries when they end a sentence.
- Honor `--no-frame-pacing` from the command line. On Wayland with EGL,
  pace swaps using the renderer’s own callback.
- Keep the pointer automatic when importing older host profiles with only
  display settings saved. Explicit Game pointer choices stay saved.
- Leave the game pointer warning out of desktop streams.
- Build and cache the `vainfo` diagnostic separately. Ship its license in the
  Flatpak.
- Try a lower startup bitrate before reducing the picture on hosts that
  accept bitrate only at launch. Learn from the next normal connection.
- Respect the automatic port-check preference after stream failures. Explain
  whether video failed to arrive, arrived incomplete, or was stopped by the host.
- Keep a chosen resolution and frame rate while learning a recommended bitrate.
  Report the negotiated audio and video encryption in the stream log.
- Keep live bitrate control responsive to packet loss after a previous change
  has settled. Distinguish the frame-rate target from rendered fps in Stream
  Controls, and label network round trip and bitrate targets.
- On Wayland, hold an incoming clipboard update until the stream can apply it
  after a local selection change. Keep a newer local copy in preference to
  an older pending update.
- Skip unnecessary HEVC and AV1 renderer probes when the host does not offer
  those codecs for an Automatic connection.

## 0.3.1-alpha.17 (private tester build, 2026-11-16)

- Send the standard display-mode request in Automatic even if an older
  preference disabled it. GeForce Experience keeps its resolution checks.
- Honor explicit CLI options when preparing an Automatic session. Safe
  retries keep their own bounds.
- Show a host warning when it reports that a virtual display was disabled.
  The stream continues with the display the host provides.
- Return to the existing paired host protocol after the alpha.16 experiment.
  Windows display preparation still depends on host behavior.

## 0.3.1-alpha.15 (private tester build, 2026-11-15)

- Keep an established stream connected while tuning. Queue picture changes
  and reversals for the next connection. **Apply Now** reconnects only when
  selected. Recovery after a real drop keeps the current picture.
- Keep separate trial baselines for each host build, route, display, and
  codec. Expire recommendations after a day.
- Adjust bitrate live on supported paths. Save a target for the next
  connection when a host may restart its encoder.

## 0.3.1-alpha.14 (private tester build, 2026-11-15)

- Offer **Take Over Desktop?** when a compatible host reports that another
  device holds the Desktop session.
- Recheck the paired device, the running target, and the host's controls
  before ending the old session. Check that it ended before starting a new
  Desktop. Check ownership after launch where the host reports it.
- Keep measurements from a new main Desktop separate from older shared
  display observations. Keep the standard launch path for other hosts.

## 0.3.1-alpha.13 (private tester build, 2026-11-13)

- Make Connect open the main Desktop. End this client's auxiliary display
  first when the host still has one from an earlier session.
- Close the HTTP connection between paired requests to avoid reusing a
  connection that lost its client-certificate identity on the host.

## 0.3.1-alpha.12 (private tester build, 2026-11-12)

- Try a separate Remote Monitor on compatible hosts, with its own launch,
  reconnect, and cleanup. Keep the normal Desktop path on other hosts.
- Resolve targets from the current authenticated application list. Send
  advertised application UUIDs alongside numeric IDs and reject malformed
  lists during role and cleanup checks.
- Separate monitor capture could run at the requested size, but it left
  existing windows on the main desktop. Alpha.13 changes normal Connect.

## 0.3.1-alpha.11 (private tester build, 2026-11-12)

- Recognize the real Desktop entry when Vibepollo also lists a running-app
  shortcut. Treat titles padded for sorting as their unpadded names.

## 0.3.1-alpha.10 (private tester build, 2026-11-12)

- Refresh guide screenshots with synthetic data and report failed renders.
- Mask more private information in copied diagnostics and remind users to
  review summaries before sharing.
- Add clearer compatibility, installer, and security reporting instructions.
- Check source pins, unsafe Flatpak staging paths, private Git content, and
  documentation links. Install third-party notices with the Linux client.

## 0.3.1-alpha.9 (private tester build, 2026-11-06)

- Retain short-term tuning results, working modes, and encoder restart costs
  across reconnects. Stop ignored or ineffective size trials.
- Keep host desktop geometry steady during tuning when UI scale cannot be
  negotiated.
- Require authenticated paired state for remote cleanup. Authenticate fresh
  TLS connections.

## 0.3.1-alpha.8 (private tester build, 2026-11-06)

- Return to the host list on disconnect and finish remote cleanup in the
  background. Report failed cleanup afterward.
- Try bitrate before a smaller picture. Keep the requested frame rate and
  recover the previous mode if a trial fails to help.

## 0.3.1-alpha.7 (private tester build, 2026-11-05)

- Improve Desktop cleanup and recovery. Use activity and delivered cadence
  to judge stream performance.
- Follow the desktop theme in the stream controls and fix cursor mapping
  and toolbar text sizing.
- Show the host software name without its version on the host page. Add
  **Re-evaluate This Host** to clear learned performance.

## 0.3.1-alpha.6, native interface (private tester build, 2026-11-01)

- Rename Starlight to Moonwalk and Skylight to Moonwalk Host. Keep the app ID,
  pairing data, settings, and command name.
- Add layouts for GNOME and KDE Plasma. Other desktops use the
  Adwaita-style layout with compositor-managed windows. Host settings and
  dialogs share the same model across desktops.
- Add searchable Preferences, a shortcut reference with visible keys,
  host details with copyable values, and a stream summary on the host page.
- Restore Saved Settings, last-working settings, pairing feedback, and
  notifications. Remember the last selected host.
- Bring desktop styling to connection status, recovery, stream controls,
  and CLI windows. Follow right-to-left layout and reduced-motion settings.

## 0.3.1-alpha.6 (private candidate, 2026-10-28)

- Group connection and per-host settings on the host page.
- Add input release and Fit in Window or Actual Size to stream controls.
- Keep trial settings temporary until a stream has presented video reliably.
  Retain the previous working settings for recovery.
- Queue regular-file transfers with progress, cancellation, and collision
  choices. Show more specific host readiness and view-only status.

## 0.3.1-alpha.5 (private candidate, 2026-10-22)

- Recheck automatic routes per session and allow an address choice per host.
- Retry interrupted streams within a time limit. Offer safe settings, restore
  the last healthy settings, and save named presets.
- Wait for a woken host to become ready. Let the user resume or end the
  application it actually reports.
- Add in-stream controls and local Compose and IME typing. Add scale
  negotiation, adaptive bitrate, PNG clipboard exchange, and regular-file
  transfers with compatible Moonwalk Host builds.

## 0.3.0-alpha (private tester build)

- Replace Moonlight Qt's app grid with a host list and per-host settings.
  Update keyboard navigation, theming, and window behavior.
- Introduce the Flatpak with a Moonlight FFmpeg build, video acceleration
  libraries, the older Intel VA-API driver, and GNOME Wayland decorations.
- Compile QML ahead of time and disable its disk cache to avoid stale
  interface files after Flatpak updates.
- Add a local cursor preference and coordinate the cursor with Skylight.
  Choose new stream sizes with display and decoder limits in mind.
- Give the app its own configuration identity and add a copy-only import
  from Moonlight. Improve PIN pairing and error handling.
- Keep brightness and volume keys local, improve address selection, and
  move clipboard long polling off the launcher thread.
