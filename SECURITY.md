# Security

Moonwalk is a private alpha for computers and networks you trust. During a
stream, the host receives your input and sends video, audio, and any enabled
clipboard or file data. Pair only with the host you intend to use. Moonwalk
pins the host certificate during pairing and checks it on later paired
requests.

Video encryption depends on the host's policy and the negotiated connection.
Open **Stream Controls** to check whether the video is encrypted. Keep host
ports off the public internet. For remote access, use a private network route
such as a VPN.

Moonwalk has no automatic diagnostic upload. Review copied diagnostics before
sharing. Moonwalk and its Moonlight Qt engine have had no independent security
audit for this release.

The Linux profile contains pairing credentials. Moonwalk restricts the settings
file to the account that owns it. Use **Copy Diagnostic Summary** for support
and keep the settings file private.

## Report a vulnerability

Use a private channel. While the repository is private, invited collaborators
can report through its private issues. If you need a private contact, ask for
one without describing the problem. Send the affected build, impact, and a
minimal reproduction using synthetic data once you have the channel.

Before making the repository public, verify GitHub private vulnerability
reporting or list a monitored private contact here. Keep vulnerability details,
keys, certificates, and unredacted logs out of public issues and pull requests.

Security fixes target the current alpha. Reports are handled on a best-effort
basis.
