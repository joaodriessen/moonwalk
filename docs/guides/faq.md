# Frequently asked questions

## How is Moonwalk different from Moonlight?

Moonwalk uses the [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt)
engine. It targets Linux, has GNOME and KDE Plasma layouts, and remembers stream
and input settings per host. It also uses optional features from compatible
hosts. Moonlight itself supports more client platforms and has a longer release
history.

## Will it change my Moonlight setup?

Moonwalk keeps its own settings. With the Flatpak, pair your hosts again. A
native build can import an existing Moonlight configuration on first launch
and leaves the original files in place.

## Do I need Moonwalk Host?

Use Moonwalk Host if you have access to the Mac companion alpha and want to
stream your Mac. On Windows and Linux, use a compatible host such as Sunshine.
Apollo and Vibepollo are Windows alternatives. The
[compatibility table](../compatibility.md#features-by-host) shows which extras
each host can provide.

## Can I use it over the internet?

Moonwalk has primarily been tested on local networks. A private VPN can make a
remote host reachable. Keep the host's ports off the public internet. The
**Test Internet Streaming Ports** menu entry is a diagnostic inherited from
Moonlight. See [Security](../../SECURITY.md).

## Is the stream encrypted?

It depends on what the host and client negotiate. Encrypted pairing or input
doesn't establish that video is encrypted. Open **Stream Controls** to see
whether the connection negotiated video encryption.

## Does Moonwalk send data anywhere else?

It has no account or analytics service. It can contact Moonlight's services
for mappings, compatibility information, and connection diagnostics.
[Network requests](how-it-works.md#what-moonwalk-sends-over-the-network)
describes when.

## What can the Flatpak access?

Moonwalk uses the network, the local display, and your sound service. The
`--device=all` grant includes graphics devices for hardware decoding and
input devices such as gamepads. Read-only host OS access supports Gamescope's
Vulkan layer. Moonwalk uses logind when you press a local brightness key
during a stream. Pairings and settings live in the app's Flatpak data.

To inspect an installed build's permissions, run
`flatpak info --show-permissions com.starlight.Starlight`.
[Package manifest](../../flatpak/com.starlight.Starlight.yaml)

## Does it work with games?

You can open a game on the streamed desktop and use a supported gamepad.
For a game that moves the camera with the mouse, try **Game** pointer mode
if Automatic picks the wrong behavior. Moonwalk's host page opens
**Desktop**. The command-line interface also accepts named apps.

## How do I update?

Close Moonwalk and install the newer Flatpak bundle. Existing builds use the
same app ID, so settings and pairings are kept. Moonwalk does not check for
updates itself. There is currently no Flathub release.

## Which languages are available?

Choose a language in Preferences. The list includes translations inherited
from Moonlight and partially translated Moonwalk pages. Where a translation
is missing, the app shows the English label.

## What does Moonwalk cost?

Moonwalk is free software under the [GPL-3.0](../../LICENSE).
