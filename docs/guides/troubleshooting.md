# Troubleshooting

Find what you're seeing below. If none of it helps, [open an issue](https://github.com/joaodriessen/moonwalk/issues).

## I can't find my host

Moonwalk can only find hosts by itself on the same local network.

- Check that both computers are on the **same network**. Guest Wi-Fi networks often keep devices apart.
- Check that the host is **running**. For [Moonwalk Host](https://github.com/joaodriessen/Skylight), its icon should be in the Mac's menu bar. If it isn't there, Moonwalk Host isn't running.
- Check that **Find hosts on the local network automatically** is on in Preferences.
- Give it a little time.

If adding the host by address works but discovery does not, check multicast discovery and the host firewall using the host's instructions. Do not disable the firewall or expose ports to the internet.

If it still doesn't appear, add it by address with **Add Host…**. To find the address:

- **Mac with Moonwalk Host:** Settings… → Network shows it under connection details.
- **Windows:** run `ipconfig` and look for the IPv4 address.
- **Linux:** run `ip -4 addr`.

## Pairing doesn't finish

- Enter the PIN **on the host**, not in Moonwalk.
- On a Mac, open **Pair New Device…** from the Moonwalk Host menu, select your computer's row, then type the PIN.
- On Sunshine and Apollo, the PIN goes on the **PIN** page of the host's web interface.
- If the host says there are too many pending requests, wait a few minutes, or restart the host software, and try again.
- If the host identity changed, verify on the other computer that you are connecting to the intended host before removing its old pairing and pairing again. Do not bypass certificate checks or delete all pairings to fix one connection.

## Pairing worked, but access is denied

Pairing and permission to start a stream are different. On Apollo-family hosts, check this device in the host's device settings: it needs permission to list/start the target and, for control, keyboard and mouse input. A view-only device may receive video without controlling it. Do not grant unrelated administrative permissions.

## The host is listed as Offline

The other computer may be asleep or off, or the host isn't running. If Moonwalk offers **Wake and Connect**, try that. Otherwise, wake the computer by hand and make sure the host is running. It appears as ready by itself.

## Moonwalk says "Desktop" is missing

Moonwalk connects to a target named **Desktop** on the host. Add an application with exactly that name in the host's settings, and leave its command empty. Moonwalk Host already has one.

## The stream is choppy

Start with [Smoother streaming](performance.md). The usual causes are Wi-Fi, a bitrate your network can't sustain, or hardware decoding that isn't active.

## Is hardware video decoding working?

Moonwalk warns you when it opens if it can't find a working hardware decoder. If you see that warning, or the stream is choppy, your computer may be decoding video on its processor.

- The Flatpak includes video drivers, including ones for older Intel graphics, but support still depends on your hardware.
- On NVIDIA graphics, Flatpak has to download a matching driver add-on. On a few unusual or very old setups that doesn't happen, and Moonwalk falls back to your processor.
- The `vainfo` command isn't a reliable test here. It can report a failure on Wayland even when decoding works fine during a stream.

If you report this, please include the diagnostic summary described [below](#getting-information-for-a-bug-report).

## My keyboard or mouse isn't reaching the host

Input may be released. Click the stream, or press **Ctrl+Alt+Shift+Z**. Input also pauses while Stream Controls is open. Press Esc to go back.

If you're connecting to a Mac and nothing reaches it at all, check that Moonwalk Host has the Accessibility permission.

## Shortcuts like Alt+Tab act on my own desktop

Press **Ctrl+Alt+Shift+K**, or turn on **Capture system keyboard shortcuts** in Preferences.

## Paste doesn't work

See [Copy and paste](while-streaming.md#copy-and-paste). On Wayland, click in the stream before you copy, and click in the Moonwalk window after disconnecting.

## The window doesn't look right on GNOME

This only affects builds you've compiled yourself. If the window has a plain, flat frame instead of a proper title bar and shadow, a small piece GNOME needs for its title bars is missing. It does no harm, and the Flatpak includes it. See [Building from source](../building.md#a-native-looking-window-on-gnome).

## Getting information for a bug report

1. After the problem happens, open Moonwalk's main menu and choose **Copy Diagnostic Summary**. It includes the version and your recent app log. Current source builds hide IPv4/IPv6 addresses, URL credentials, common secret fields, home-directory names and recognizable certificate/key blocks. Older builds may hide fewer details. Host names and other identifying details can still appear; this is not a guarantee of anonymization.
2. Read it before you share it, and remove anything private, such as host names.
3. [Open an issue](https://github.com/joaodriessen/moonwalk/issues). The form asks for the rest.

Your version number is under **About Moonwalk** in the main menu. From a terminal, you can also get a summary of your system with:

```bash
flatpak run com.starlight.Starlight --diagnostics
```

That starts a new copy of Moonwalk, so it doesn't know about your recent connections.

---

[All guides](README.md) · [Questions and answers](faq.md)
