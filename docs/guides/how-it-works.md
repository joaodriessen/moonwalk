# How Moonwalk works

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/how-it-works-dark.svg">
    <img src="../assets/how-it-works-light.svg" width="720" alt="A host sends picture and sound to the Linux client. The client sends keyboard, mouse, and gamepad input to the host.">
  </picture>
</p>

The host captures its desktop and encodes the picture. Moonwalk receives the
video and audio, decodes them, and plays them on your Linux computer. Input
travels in the other direction. On a suitable setup, the host and client GPUs
do the video encoding and decoding. Software paths also exist, depending on
the hardware, drivers, and codec.

Enter Moonwalk's PIN in the host app. This lets the host recognize the client
on later connections and apply the permissions for that device.

## Terms

| Term | Meaning |
| --- | --- |
| **Host** | The computer whose desktop you use, or the software sharing it. |
| **Client** | The device running Moonwalk. |
| **Bitrate** | Data per second, usually in Mbps. A higher video bitrate gives the encoder more room to preserve detail and needs more network capacity. |
| **Codec** | The video compression format, such as H.264, HEVC, or AV1. Both ends need to support it. |
| **Virtual display** | A screen the host creates in software. It can have a different size or refresh rate from the physical monitor. |
| **Moonlight-compatible** | Uses the streaming protocol Moonlight clients speak. Extensions and permissions vary by host. |

## Flatpak permissions

The Flatpak includes video libraries and some drivers. GPU support still
depends on the installed system driver. The package has network, display, audio,
and device access, including gamepads. It can read system files for graphics
integration, and it can ask the desktop to keep the screen awake. The manifest
also includes access to the Gamescope socket and login manager. Your home
directory isn't granted wholesale. The desktop file picker can grant access to
files you choose. See the [manifest](../../flatpak/com.starlight.Starlight.yaml)
for the full list.

## What Moonwalk sends over the network

Moonwalk uses your network to find hosts, pair, and stream. It can also:

- Download controller mappings from `moonlight-stream.org`. A paired NVIDIA
  host can also trigger a GeForce Experience compatibility update.
- Look up a fallback public IPv4 address through `stun.moonlight-stream.org`
  after some IPv4 host additions. Local pairing can proceed during the lookup.
- Check streaming ports using `qt.conntest.moonlight-stream.org`. This can
  happen when adding a host, through **Test Internet Streaming Ports**, or after
  certain stream failures.

These services receive normal connection information, such as your public
address. Turn off **Detect blocked connections automatically** to skip
automatic port checks and fallback address lookups. You can still run
**Test Internet Streaming Ports** yourself. Moonwalk has no analytics service.

When you connect to a host, permissions and pairing determine which data is
sent there. See [Security](../../SECURITY.md) for what to expect from encryption
and [Compatibility](../compatibility.md#features-by-host) for clipboard,
file transfer, and display features.
