# Troubleshooting

## I can't find my host

Make sure both computers are on the same local network and the host software is
running. A guest Wi-Fi network can keep devices from reaching each other.
Check **Find hosts on the local network automatically** in Preferences.

If the host doesn't appear, enter its local address with **Add Host…**.
On Moonwalk Host, copy an address from **Settings… → Network**. On macOS you
can also find the address in **System Settings → Network**.
On Windows, run `ipconfig` and look for the active adapter's IPv4 address. On
Linux, run `ip -4 addr`. If the address works but discovery still fails,
check multicast and firewall settings with the host's documentation.

## Pairing doesn't finish

Enter the PIN on the host. On Moonwalk Host, choose **Pair New Device…** from
its menu bar icon and select your device. On Sunshine, Apollo, and Vibepollo,
use the host web interface's **PIN** page.

If the host reports too many pending requests, wait for them to expire and
try again. If Moonwalk reports that the host's identity changed, verify you're
connecting to the intended machine before removing that pairing and pairing it
again.

## Pairing worked, but access is denied

The host can authorize a device to pair without letting it launch a stream or
send input. On Apollo-family hosts, check that your paired device can see and
start **Desktop** and has keyboard and mouse permission if you need control.

## The host is Offline or In use

If the host is asleep, use **Wake and Connect** if it appears. Otherwise wake
it at the machine and check that its host software is running.

**In use** means the host still reports a running session. Wait a moment if
you just disconnected. If the host kept the session open, it may offer to
resume it. **Take Over Desktop?** asks the host to end the existing Desktop
before starting a fresh one. It can disconnect other viewers.
[Which desktop you get](../compatibility.md#which-desktop-you-get)

## Moonwalk can't find Desktop

Moonwalk opens an application named **Desktop** on the host. In the host's
application settings, add that entry with no launch command if it's missing.
Moonwalk Host already includes it.

## The picture is choppy or the wrong size

See [Streaming performance](performance.md) for how to check whether the
host, network, or client is limiting the stream. For a desktop that's too
large or small, check [Display settings](settings.md#display). A stream can
be the right pixel size while the host's text scale is wrong.

## Is hardware video decoding working?

Moonwalk can warn when it can't find a hardware decoder for the requested
codec. The Flatpak includes video libraries and drivers for some older Intel
GPUs.
NVIDIA graphics can require a Flatpak driver extension that matches the
installed driver. Availability depends on the codec and the selected renderer
as well as the GPU. The `vainfo` command only tests a VA-API configuration.
Moonwalk may use a different path for the stream.

Include your GPU and the diagnostic summary if you report a decoding problem.

## My keyboard or mouse isn't reaching the host

Click the picture or press **Ctrl+Alt+Shift+Z**. Input stays local while
Stream Controls is open. Press **Esc** to return. If input never reaches a
Mac host, check Moonwalk Host's Accessibility permission in macOS.

If Alt+Tab still acts on your own desktop, try **Ctrl+Alt+Shift+K** or
**Capture system keyboard shortcuts** in Preferences. The desktop may reserve
some shortcuts.

## Paste doesn't work

Check the clipboard direction in Stream Controls. After another local app
changes the clipboard, Wayland may wait for your next click or keystroke in
the stream. If paste stops working after disconnect, click the Moonwalk
window and try again. See
[Copy and paste](while-streaming.md#copy-and-paste).

## The window doesn't look right on GNOME

If your native build has a plain frame, check that the Wayland decoration
plugin is present and that a Qt environment override isn't selecting another
platform. See [GNOME window decorations](../building.md#gnome-window-decorations).

## Getting information for a bug report

After the problem happens, open the main menu and choose **Copy Diagnostic
Summary**. It includes your build and recent Moonwalk log lines. Moonwalk
masks common addresses, paths, and credentials, but host names or other
identifying details can still appear. Read the text and remove anything
private before you [open an issue](https://github.com/joaodriessen/moonwalk/issues/new/choose).

Look under **About Moonwalk** for the version. If the app can't open, this
command prints a local summary and exits:

```sh
flatpak run com.starlight.Starlight --diagnostics
```

The terminal command starts a separate process, so it doesn't have the recent
log from the window you were using.

[All guides](README.md)
