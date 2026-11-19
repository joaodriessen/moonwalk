# Who controls stream settings

Moonwalk picks a stream mode from the local display, your preferences, and
the decoder. The host applies its own capture, encoding, and permission
limits before the stream starts.

| Setting | Moonwalk | Host |
| --- | --- | --- |
| Resolution and frame rate | Plans a mode from the local display, per-host settings, and decoder limits | Sets up the capture and encoder within its capabilities |
| Bitrate | Sets an initial target and can adjust it over a supported live control | Accepts, clamps, or rejects it. Some encoders restart to apply it. |
| Codec | Chooses from advertised modes it can decode | Advertises supported codecs and encodes the negotiated format |
| Text size | Requests a supported desktop scale when available | Lays out the desktop. A host can scale the video separately. |
| Cursor | Chooses absolute or relative input and whether to draw a local cursor | Captures its own cursor or coordinates with the client where supported |
| Clipboard and files | Offers directions and transfer controls when available | Grants access per paired device and applies local permissions |

Keep standard launch and resume working when optional fields are absent.
Check advertised support and permissions for each extension. A name such as
Sunshine, Apollo, or Vibepollo is useful for display and diagnostics, but
can't establish permission for a particular operation.

Moonwalk uses the host's existing Desktop entry and pairing. Host policy
controls whether a virtual display can become the main desktop and how the
display is restored. An existing session may require explicit takeover.
See [Compatibility](../compatibility.md#which-desktop-you-get) for the
session behavior exposed to users.
