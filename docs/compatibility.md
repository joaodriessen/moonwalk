# Compatibility

Moonwalk is in alpha. This page covers the current source. Check
**About Moonwalk** and the [changelog](../CHANGELOG.md) if you're using an older
build.

## Where it runs

| Setup | What we've checked |
| --- | --- |
| **Linux x86-64 Flatpak** | The primary package. Hardware decoding depends on your GPU and drivers. |
| **GNOME and KDE Plasma on Wayland** | The installed Flatpak streamed on both desktops. The GNOME test opened Settings through its accessibility menu and the Plasma test mapped the pointer across a 2× display. Physical input and GPU coverage are still limited. |
| **Hyprland and other desktops** | Moonwalk uses the generic layout here. The installed Flatpak streamed under nested Hyprland. Its local cursor matched the desktop cursor in height at 2×, and closing the window ended the session. Other compositors may behave differently. |
| **X11** | Available as a fallback. Coverage is more limited than on Wayland. |
| **Native Linux build** | Tested on Ubuntu 24.04 with Qt 6.4.2. The Flatpak has its own Qt runtime. See [build instructions](building.md) for other distributions. |
| **Other client platforms** | Moonwalk targets x86-64 Linux. See [Moonlight](https://moonlight-stream.org/) for clients on other platforms. |

| Host | Streaming checks |
| --- | --- |
| Vibepollo on Windows | Pairing, streaming, and disconnect cleanup on real machines. Full-rate streaming at native resolution remains unresolved on one Intel host. |
| Moonwalk Host on Apple Silicon | Pairing and live desktop streams with the separate macOS 14+ host. |
| Sunshine on Linux | The Moonwalk Flatpak passed PIN pairing, stereo sound, reconnect, recovery after the client was killed, and an installed upgrade that retained the pairing and saved display settings. The stock Sunshine 2026.914.233613 AppImage passed X11 streams at 1280×720 with software H.264 at 60 fps and HEVC at 30 fps. An AV1 request fell back to H.264 on that host. Pairing and reconnect also passed over IPv6 loopback. |
| Apollo on Windows | Uses the same base protocol. Current releases and optional features need more testing with Moonwalk. |

Sunshine 2025.924.154138 also passed PIN pairing, 1280×720 H.264, ordinary
reconnect and reconnect after the client was killed. That build could not
create its virtual keyboard and mouse in the test environment.

A separate stock Sunshine trial added delay and jitter in both directions.
With a measured 24–25 ms round trip, graceful sessions rendered about 60 fps
at 1280×720. Pairing and reconnect after a forced client exit also passed.

Most testing has been on a local network. Moonwalk has no built-in relay or
internet setup. A private VPN can provide a route to the host, but we haven't
qualified that setup.

A separate controlled 1280×720 AV1 stream at 30 fps exercised Moonwalk's
software decoder and presentation on Plasma Wayland. Stock Sunshine tests
confirmed negotiated LAN video encryption with the host set to **Required**
and unencrypted video with that setting disabled. Audio stayed encrypted in
both tests.

## Features by host

Standard video, audio, and input use the host's existing pairing and
permissions. Optional features depend on what the installed host version
advertises and allows for your paired device.

| Feature | Moonwalk Host | Apollo and Vibepollo | Other compatible hosts |
| --- | --- | --- | --- |
| Automatic text clipboard | Yes | With the clipboard extension and permission | Type local clipboard text with Ctrl+Alt+Shift+V |
| Image clipboard and file transfer | Yes | — | — |
| Host cursor shapes | Yes | Local cursor | Local cursor |
| Choose the size of text and UI on the host | 1× or 2× on supported modes | Use the host's display settings | Depends on the host |
| Change bitrate in a stream | Yes | With the bitrate control extension | Depends on the host |
| Virtual display | Yes | Depends on driver, policy, and permission | Depends on the host |

A host that accepts live bitrate changes can still pause video briefly to restart
its encoder. HDR, 4:4:4, surround sound, high refresh rates, and gamepads depend on
the whole chain from host to display. Moonwalk Host targets SDR and stereo.
It can emulate a keyboard and mouse when a macOS virtual gamepad is unavailable.

## Which desktop you get

Normal **Connect** opens the host's existing **Desktop** entry. Automatic mode
asks for the client-sized mode and lets the host prepare that display. A ready
virtual display may become the main screen for the session if the host's policy
allows it. Moonwalk keeps the separate Remote Monitor role for explicit requests.

A shared or retained Desktop may still use the previous client's display. If
the host advertises the controls needed to end that session, Moonwalk can offer
**Take Over Desktop?** before connecting. Approving this can disconnect other
viewers. Moonwalk rechecks the running session, uses the host's confirmation if
required, and checks that the session ended before asking for a fresh Desktop.
If a check fails or the session changes, the attempt stops. Hosts without those
controls may need the original client to end the retained session. The host
handles display preparation and restoration.

The resolution you receive is the size of the encoded picture. A host can
resize a smaller desktop before encoding it. The FFmpeg decoder can report
the received picture size. Windows text size is set on the host, and the
standard protocol can't verify the capture size or effective UI scale. If the
host reports that it disabled a virtual display, Moonwalk shows the warning
and continues with the picture the host provides.

In isolated GNOME and Plasma tests, a 2880×1800 display at 2× scaling
requested 2880×1800 at 60 fps with 2× desktop scaling from a host that
advertised the scale extension. The same Plasma setup mapped pointer motion
across the full picture. On a 2256×1504 Plasma display at 1.5×, pointer motion
also reached all four corners of a letterboxed 1280×720 stream. In a separate
GNOME test at 1920×1200 on a 2× display, Moonwalk requested 1× host scale
and kept the 60 fps stream target. Display placement and restoration still
need tests on the actual host OS.

## Performance and disconnect

Automatic starts with the client display's size and refresh rate, subject to
host and decoder limits. It uses measured delivery to recommend changes and
preserves the requested frame rate during resolution trials. It keeps a working
stream connected. See [Performance](guides/performance.md) for what happens
during the stream and on the next connection.

Disconnect closes the stream and returns to the host list while Moonwalk
requests remote cleanup. If the network or host goes offline, the Desktop may
stay resumable until the host can be reached or expires it. A crash can leave
the same kind of retained session.

If a setup fails, [report it](https://github.com/joaodriessen/moonwalk/issues/new/choose)
with both software versions, the client desktop and GPU, host OS and GPU,
display size and refresh, and the symptom you see. Review diagnostics for
private information before sharing.
