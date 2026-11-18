# Compatibility and current limits

Moonwalk is a private alpha. **No combination is certified as a stable public
release yet.** This matrix describes the current development line, not every
older download. Check the version in About and the release notes.

## Client and host scope

| Area | Status | What that means |
| --- | --- | --- |
| Linux x86-64 Flatpak | Supported development target; experimental product | Primary packaging path. Hardware decoding still depends on GPU and driver support. |
| GNOME and Plasma on Wayland | Experimental; controlled native checks performed | Both use the same settings model. Physical input, accessibility and hardware coverage remain incomplete. |
| Hyprland and other desktops | Experimental | Generic layout and compositor-managed windows; some Hyprland checks exist, not qualification of every compositor. |
| X11 | Experimental fallback | Not equivalent to full Wayland qualification. |
| Native Linux builds | Contributor path | Qt-floor CI is distinct from the shipped Flatpak runtime. Fedora/Arch instructions are not certification of each distro release. |
| Windows/macOS/mobile/ARM clients | Unsupported release targets | Inherited Moonlight code does not establish Moonwalk support. |
| Moonwalk Host on Apple Silicon | Experimental companion | macOS 14 or later is the target; one Mac's results do not qualify every supported OS version. |
| Vibepollo 2.0.1 on Windows | Limited real-stream checks | Pairing, streaming and cleanup have been exercised. Native-resolution smoothness on a constrained Intel host remains unresolved. |
| Sunshine and Apollo | Protocol-compatible targets; current version matrix unverified | Standard streaming is intended; each optional extension needs its own capability and permission checks. |
| Internet streaming, relays and multi-user hosting | Unsupported product scope | Local-network alpha. No built-in traversal or relay service; VPN routes are not a general WAN qualification. |

## Optional features

Standard streaming uses ordinary PIN pairing. Moonwalk does not require an
additional administrator sign-in or a companion service beside an existing
compatible host. Automatic display setup sends the standard launch request for
the chosen mode; a hidden legacy game-optimization preference cannot disable it.
GeForce Experience retains its supported-resolution guard. Explicit command-line
options still take precedence for that connection.

| Feature | Moonwalk Host | Apollo / Vibepollo | Other compatible hosts |
| --- | --- | --- | --- |
| Standard desktop video, audio and input | Yes, within alpha scope | Subject to host permissions | Intended standard protocol path |
| Automatic text clipboard | Extension supported | Compatible versions and permissions | Type/paste shortcut fallback |
| Image clipboard and regular-file transfers | Extension supported | Not implemented by Moonwalk for these hosts | Not implemented |
| Remote cursor shapes | Extension supported | No equivalent Moonwalk integration | Local cursor fallback |
| Desktop UI scale negotiation | Supported 1×/2× modes | No generic effective Windows UI-DPI contract | Host-dependent |
| Live bitrate control | Compatible host extension | Capability-gated; some encoders restart briefly | Not assumed |
| Virtual display | Host extension | Host version, driver, policy and permissions | Host-dependent |

Normal Connect targets the host's existing Desktop application. Automatic virtual
display requests go through that application's display policy; they do not select
an independent additional Monitor. An exclusive client-sized host display can
bring the existing windows onto the streamed screen, with restoration performed
by the host. That behavior depends on the host's supported policy, current session
ownership and driver. A retained/shared Desktop cannot always be resized by a
secondary client. Moonwalk must not silently terminate another client's game to
force a display change.

The UUID-identified Remote Monitor protocol remains supported for explicit role
requests. Its client-scoped disconnect is kept separate from global application
cancellation. When normal Connect encounters this client's retained auxiliary
role, it releases that role and resolves the normal Desktop from a fresh,
authenticated catalogue. Unknown hosts retain standard launch/resume behavior.

For hosts advertising the UUID-identified running-session and Terminate controls,
Moonwalk detects a Desktop retained by another device before opening video. It
asks **Take Over Desktop?**, then uses the advertised host control, checks current
paired identity/permissions and catalogue, handles at most one host confirmation,
and checks that the prior session ended before launching a fresh main Desktop.
It verifies ownership on hosts that project running state per paired client.
Missing controls, unexpected replies or observed session changes stop the attempt;
there is no fallback to an additional monitor or unguarded global cancellation.
Ordinary Sunshine/GFE launch/resume semantics remain unchanged.

This is a stock-host control sequence, not an atomic expected-target transaction.
Approval can disconnect other viewers of the current streaming application, and
legacy APIs cannot eliminate changes occurring between requests. Moonwalk never
infers approval from the host advertising itself as free. Hosts without these
controls may need their retained session ended from the original client. Host
configuration/driver policy still determines exclusive display setup, UI scale
and restoration. No host patches, SSH-per-connection action or service restart is
part of this client flow.

The encoded frame size does not prove the host captured a desktop at that size:
a host may resize a physical/shared/retained desktop before encoding. There is no
universal Moonlight capture-size or Windows UI-DPI acknowledgement. Private-role
support, legacy protocol tests and one host's field result do not certify every
host/version, concurrent same-client process, driver or display topology.

Text clipboard compatibility does not imply image/file compatibility. A host's
brand or version string does not grant permissions or prove an extension exists.
Host runtime-bitrate acknowledgements do not guarantee a pause-free encoder.

Automatic targets local geometry and refresh, prioritizes scaling and usable
motion, and bounds tuning trials. It cannot guarantee native resolution at full
refresh on every machine. On hosts without UI-scale negotiation, keeping desktop
geometry fixed during tuning protects the starting layout; it cannot repair an
initially incorrect host UI scale. Decoded-picture size verification currently
uses the FFmpeg path; other decoders retain negotiated-size fallback. This does
not verify the host's capture source or effective UI scale.

Moonwalk reads the optional stock-host warning
`virtual_display_disabled_by_client` on successful launch/resume responses and
keeps it in the existing stream configuration warnings. It does not disconnect
or retry in response. Older hosts that do not send this warning remain
compatible; absence of a warning does not verify successful display setup.

Normal disconnect returns to the launcher while authenticated cleanup runs in
the background. A crash or offline client can leave a target resumable until
recovery. Universal host expiry, atomic multi-client ownership and display
restoration are not yet guaranteed.

HDR, 4:4:4, surround sound, high-refresh displays and controllers depend on the
whole host/codec/driver/client path. They are not qualified simply because the
engine contains support. Moonwalk Host currently offers SDR/stereo; its gamepad
input maps to keyboard/mouse rather than a virtual gamepad.

## Expanding support

A useful report includes both build versions, client GPU/driver and desktop,
host OS/GPU, display size/scale/refresh, connection type and the actual failure.
Use the [bug form](https://github.com/joaodriessen/moonwalk/issues/new/choose)
and review diagnostics for private information before sharing them.

Qualification should cover representative Intel/AMD/NVIDIA clients, fractional
scaling, external-display switching, permissions, keyboard layouts, audio-device
changes, reconnects and clean installs. A passing unit test, nested compositor
or single hardware pair is evidence for that case, not universal support.

[Getting started](guides/getting-started.md) · [Release process](releasing.md)
