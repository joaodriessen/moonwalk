# Security

Pair with a host you recognize. Moonwalk saves its certificate during pairing
and checks it on later paired requests. During a stream the host sees your
input and sends you video and audio. Clipboard and file data can also travel
between the two devices when you enable those features.

Video encryption depends on the host's policy and the negotiated connection.
Open **Stream Controls** to check whether the video is encrypted. Keep host
ports off the public internet. For remote access, use a private network route
such as a VPN.

**Copy Diagnostic Summary** copies text to your clipboard. Read it before
sharing it. This alpha and its Moonlight Qt engine have had no independent
security audit for this release.

The Linux profile contains pairing credentials. Moonwalk restricts the settings
file to the account that owns it. Use **Copy Diagnostic Summary** for support
and keep the settings file private.

## Flatpak permissions

The Flatpak uses network access for host discovery and streaming, Wayland or
X11 for the window and input, and PulseAudio for playback. The package grants
access to all devices for hardware decoding and controllers. It has read-only access
to host OS files for the Gamescope compatibility layer. During a stream,
Moonwalk can use the system session service to handle local brightness keys.

The KDE runtime adds access to read Plasma appearance settings and receive
their changes. The [bundle verifier](docs/releasing.md#build-and-test-the-candidate)
lists the permissions read from the exported file.

## Report a vulnerability

Use GitHub's [private vulnerability reporting form](https://github.com/joaodriessen/moonwalk/security/advisories/new).
Include the affected build, impact, and a minimal reproduction using synthetic
data. If you need another way to report privately, open an issue asking how to
contact the maintainer. Keep vulnerability details, keys, certificates, and
unredacted logs out of public issues and pull requests.

Security fixes target the current alpha. Reports are handled on a best-effort
basis.
