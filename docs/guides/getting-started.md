# Getting started

Install Moonwalk on the Linux computer in front of you. Install or start a
host app on the computer you want to control. Then pair them on the same
local network.

## Set up a host

### For a Mac

Install and open **Moonwalk Host** on the Mac you want to use. It needs Apple
Silicon and macOS 14 or later. Follow its setup window and leave the Mac
awake and logged in. The host runs from the menu bar.
[Set up Moonwalk Host](https://github.com/joaodriessen/moonwalk-host/blob/main/docs/INSTALL.md)

### For Windows or Linux

Start [Sunshine](https://github.com/LizardByte/Sunshine) on Windows or Linux,
or use [Apollo](https://github.com/ClassicOldSong/Apollo) or
[Vibepollo](https://github.com/Nonary/Vibepollo) on Windows. If you're already
streaming with one of those hosts, use it here too.

Moonwalk opens the **Desktop** entry. If your host's app list is missing it,
add an entry called **Desktop** and leave the launch command blank.

## Install Moonwalk

Download the Flatpak bundle from [GitHub Releases](https://github.com/joaodriessen/moonwalk/releases)
or [build from source](../building.md).
If you don't have Flatpak yet, follow [the setup instructions](https://flatpak.org/setup/)
for your distribution.

In the directory containing the download, run these commands with your
bundle's filename. Older tester builds may start with `Starlight`.

```sh
flatpak install --user ./Moonwalk-VERSION.flatpak
flatpak run com.starlight.Starlight
```

If Flatpak can't find the runtime, enable Flathub and install the branch named
in the error. Current bundles use
`flatpak install --user flathub org.kde.Platform//6.11`. Some older builds use
6.10.
You can also open **Moonwalk** from your app launcher after installation.

## Find the host

With both computers on the same local network, open Moonwalk. Hosts it
discovers appear in the sidebar.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/first-launch-dark.png">
    <img src="../assets/first-launch-light.png" width="620" alt="Moonwalk's empty host list with Add Host and Set Up a Host buttons.">
  </picture>
</p>

If the host doesn't appear, use **Add Host…** and enter the other computer's
address. See [finding your host](troubleshooting.md#i-cant-find-my-host) for
where to look up the address.

## Pair and connect

1. On Linux, select the host and click **Pair**. Leave the four-digit PIN
   visible while you go to the other computer.
2. Enter the PIN on the host using the screen for your host below.
3. Back in Moonwalk on Linux, click **Connect** after pairing succeeds.

| Host app | Where to enter the PIN |
| --- | --- |
| **Moonwalk Host** | On the Mac, open its menu bar icon, choose **Pair New Device…**, and select the pending device. |
| **Sunshine, Apollo, or Vibepollo** | On the host computer, open **PIN** in the host web interface. Sunshine usually uses `https://localhost:47990` on that computer. Sign in there if the host asks. |

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/pin-dark.png">
    <img src="../assets/pin-light.png" width="620" alt="The PIN dialog for an example host called Lounge.">
  </picture>
</p>

**Resume Desktop** means the host still has a Desktop target running.
**Switch to Desktop** means another application is running. Read the
confirmation before ending or taking over an existing session.

The host page shows the resolution, frame rate, and bitrate Moonwalk plans
to request. Start with **Automatic**. The host and decoder can limit what the
stream actually delivers.

## During the stream

Press **Ctrl+Alt+Shift+Z** to release your keyboard and mouse. Click the
picture to control the host again. Press **Ctrl+Alt+Shift+Q** to disconnect,
or open the controls at the top of the picture and choose **Disconnect**.

Moonwalk returns to the host list while it asks the host to end the Desktop
stream. You stay logged in on the host. See
[stream controls and shortcuts](while-streaming.md) for details.

## Update or remove Moonwalk

Close Moonwalk before installing a newer bundle with the same command.
Settings and pairings use the same Flatpak app ID. Install each new tester
bundle by hand.

This command removes the user installation and keeps your Moonwalk data.

```sh
flatpak uninstall --user com.starlight.Starlight
```

To go back to a previous tester build, close Moonwalk, remove it using the
command above, and [install the previous bundle](#install-moonwalk). Keep a
copy of your settings if the older build predates a settings migration.

Add `--delete-data` only if you also want to remove Moonwalk's settings,
hosts, and pairings.

[Settings](settings.md) · [Troubleshooting](troubleshooting.md) · [All guides](README.md)
