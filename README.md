# Moonwalk

Moonwalk started as a Linux client for remote access to macOS. It uses the
[Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt) streaming
engine and pairs with [Moonwalk Host](https://github.com/joaodriessen/moonwalk-host)
on the Mac. It also works with existing Moonlight-compatible hosts such as
Sunshine.

## Setup

Moonwalk targets x86-64 Linux, with Flatpak as the primary package. The
macOS host runs on Apple Silicon with macOS 14 or later. Both projects are
in alpha, and tester builds are distributed privately.

- [Install Moonwalk on Linux](docs/guides/getting-started.md#install-moonwalk)
- [Install Moonwalk Host on macOS](https://github.com/joaodriessen/moonwalk-host/blob/main/docs/INSTALL.md)
- [Build Moonwalk from source](docs/building.md)

For a Mac on the same network, select it in Moonwalk and choose **Pair**.
Enter the PIN in **Pair New Device…** on the Mac, then choose **Connect** in
Moonwalk. The setup guide covers adding a host by address and pairing with
other host apps. [Setup guide](docs/guides/getting-started.md)

## Stream behavior

Automatic uses the local display's resolution and refresh rate as the
starting point, within host and decoder limits. On hosts with live bitrate
control, Moonwalk adjusts bitrate during the stream. It queues recommended
resolution changes for the next connection. Settings are saved per host.

With Moonwalk Host, the Mac can create a virtual display for the requested
mode. Compatible builds support desktop scale requests, clipboard text and
images, and file transfer. **Disconnect** closes the local stream window
while Moonwalk requests cleanup from the host. The Mac user session stays
logged in. [Display and host compatibility](docs/compatibility.md) ·
[Stream controls](docs/guides/while-streaming.md)

Moonwalk uses the existing pairing flow and **Desktop** entry on Sunshine
(Windows or Linux), Apollo, and Vibepollo (Windows). Optional features
depend on the host and the permissions it grants the client. Moonwalk has
GNOME and KDE Plasma layouts and a generic layout for other Linux desktops.
[Features by host](docs/compatibility.md#features-by-host)

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/hosts-dark.png">
  <img src="docs/assets/hosts-light.png" width="700" alt="Moonwalk on GNOME showing the planned stream settings for an example Mac.">
</picture>

## Documentation

[User guides](docs/guides/README.md) ·
[Troubleshooting](docs/guides/troubleshooting.md) ·
[Development](docs/development.md) ·
[Contributing](CONTRIBUTING.md) · [Changelog](CHANGELOG.md) ·
[Security](SECURITY.md)

Moonwalk is licensed under [GPL-3.0](LICENSE) and uses icons from
[Lucide](https://lucide.dev).
[Upstream history](docs/upstream.md) · [Third-party notices](docs/third-party-notices.md)
