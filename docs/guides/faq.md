# Questions and answers

## What does Moonwalk do?

It lets you see and control another computer from your Linux desktop. The other computer does the work and sends a video of its screen. Your keyboard, mouse and gamepad are sent back.

## How is it different from Moonlight?

Moonwalk is built on [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt) and uses its streaming engine, so it talks to the same hosts. The differences are:

- It's only for Linux.
- It has a new interface made for GNOME and KDE Plasma.
- Each host has its own settings.
- It works with Moonwalk Host for extras such as files, clipboard and cursor shapes.

Moonlight runs on many more devices and has a much longer track record. If you need something proven, or you're not on Linux, use Moonlight.

## Will it change my Moonlight setup?

No. Moonwalk keeps its own settings and leaves Moonlight's alone. The Flatpak can't read Moonlight's settings, so you'll pair each host again.

## Do I need Moonwalk Host?

Only if the other computer is a Mac. Moonwalk also works with Sunshine, Apollo, Vibepollo and other Moonlight-compatible hosts. Some extras need Moonwalk Host. See the [table in the README](../../README.md#what-you-can-do).

## Is it free?

Yes. It's free software under the GPL-3.0 license, and it has no accounts or subscriptions.

## Can I use it over the internet?

It's made for your local network, and that's what has been tested. Moonwalk has a **Test Internet Streaming Ports…** tool inherited from Moonlight, and connecting by a remote address may work, but it's untested here. If you try it, use a private tunnel such as a VPN, and don't open the host's ports to the internet.

## Is it safe?

A paired computer gets your keyboard and mouse input during a stream, and sends you video and audio. A malicious host could send unexpected data, so only pair with hosts you trust. By default, many hosts don't encrypt video on the local network, so use a network you trust as well. Neither Moonwalk nor its engine has had an independent audit. See [Security](../../SECURITY.md).

## Does Moonwalk send data anywhere else?

There are no accounts or analytics. Besides talking to your hosts, it makes a few functional network requests, such as downloading gamepad mappings. [How it works](how-it-works.md#what-starlight-sends-over-your-network) lists them.

## Does it work with games?

Yes. It streams whatever is on the host's Desktop target, and gamepads are supported. Set **Pointer** to **Game** if a game's camera behaves oddly. There's no game library or app picker: open games on the remote desktop as you normally would.

## Why Flatpak?

Flatpak carries the video decoding libraries with the app, so it works the same way across Linux distributions. You can also [build it yourself](../building.md), but what you need varies by distribution.

## Does it update itself?

No. Install a newer file to update. Your hosts and settings stay.

## Why is it a private alpha?

It has been used heavily on a small set of computers, but installing and streaming on other Linux hardware and desktops needs more testing.

## Which languages are available?

English is complete. French, German and Dutch can be selected, but they're partly translated and fall back to English.

---

[All guides](README.md) · [Troubleshooting](troubleshooting.md)
