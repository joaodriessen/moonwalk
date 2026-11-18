# Security

Moonwalk is alpha software for connecting to computers you control. This page explains what it trusts and how to report a problem.

## What to keep in mind

- **Only pair with hosts you trust.** During a stream, a host sends Moonwalk video, audio and other data. A malicious host could send unexpected data on purpose.
- **Pairing happens once, with a PIN.** After that, Moonwalk checks the host's certificate each time, and only a paired device can start a stream.
- **Use a network you trust.** Whether video is encrypted depends on the host. Many hosts, Moonwalk Host included, leave it off on the local network by default.
- **Don't open a host's ports to the internet.** If you need remote access, use a private tunnel such as a VPN.
- **Your diagnostics are yours.** Moonwalk never uploads them. Read a diagnostic summary before sharing it.
- **Nothing has been audited.** Neither Moonwalk nor the Moonlight Qt engine it uses has had an independent security audit.

## Report a problem

Do not put vulnerabilities, exploit details, credentials, certificates, keys, or
unredacted diagnostics in public issues or pull requests.

While this repository is private, invited collaborators may report through its
private issues. Before changing visibility, the maintainer must enable and verify
GitHub's **Report a vulnerability** route under **Security → Advisories**, or
publish a monitored private contact here. That public disclosure route is not
established yet; this is a publication prerequisite, not a claim that reports
are already accepted there.

If you cannot access a private reporting route, open an ordinary issue only to
request one, without describing the vulnerability. Wait for a private channel
before sending details.

Include the affected version/build, impact, and a minimal reproduction using
synthetic data. Reports are handled on a best-effort basis; there is no bug bounty
or guaranteed response time. Only the current alpha development line is
maintained; older private builds receive no separate security backports.
