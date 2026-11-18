# Settings

You can use Moonwalk without changing anything. This guide is for when you want to.

There are two places for settings:

- **The host page** holds settings for one computer: picture, display, keyboard and mouse. They're saved separately for each host.
- **Preferences** holds settings for Moonwalk as a whole. Open it from the main menu (it's called **Configure Moonwalk…** on KDE), or press Ctrl+,. You can search it with Ctrl+F.

## On the host page

### Stream

**Picture and bandwidth** defaults to **Automatic**. It chooses resolution, frame rate, codec and bitrate policy for this display and host. The summary is a plan, not proof that the host will deliver that frame rate. Automatic preserves your saved Manual choices for later.

Choose **Manual** to reveal these controls:

| Setting | What it does |
| --- | --- |
| **Resolution** | Choose a size, or keep matching the local display at the next connection. A manually fixed size stays until you change it. |
| **Frame rate** | Choose a rate, or match the local screen's current refresh at connection time (10–480 FPS, with 60 when unavailable). Actual delivery still depends on both machines and the network. |
| **Video codec** | Automatic, H.264, HEVC (H.265) or AV1. Availability depends on the host and decoder. |
| **Limit bandwidth** | Enables a chosen bitrate target instead of letting the quality policy choose it. |
| **Bandwidth limit** | The chosen target in Mbps. Encoded traffic varies with the picture, and transport overhead makes network traffic a different measurement. |

The summary may describe bitrate as **Automatic**, **Recommended** (when the host cannot change it live), or **Limited**. These are not three separate Stream mode buttons. Automatic can react to measured pressure where supported; it does not guarantee a stutter-free connection. See [Smoother streaming](performance.md).

### Display

**Automatic**, the default, chooses the closest supported host text size, fits the whole picture in the window and, on hosts that offer it, gives desktop sessions a display of their own. Choose **Manual** to set **Remote text size**, **View scale** and **Host display** yourself. Your Manual choices are kept if you switch back to Automatic, and come back when you choose Manual again.

| Setting | What it does |
| --- | --- |
| **Remote text size** | Moonwalk Host only. Chooses how large text and windows appear on the Mac: **Automatic**, **Small (1×)** or **Large (2×)**. This changes the Mac desktop's layout without changing how many pixels are sent. It takes effect when you connect. **Large (2×)** needs a resolution with an even width and height. |
| **Host display** | On hosts that let Moonwalk choose, such as Apollo: **Automatic**, **Use host setting**, **Dedicated virtual display** or **Physical display**. A dedicated virtual display is a separate screen just for your session. If you explicitly ask for one and the host's driver isn't ready, Moonwalk tells you rather than using another screen. The host's own settings can override this. Available choices depend on the advertised capability; the host's display policy still applies. |
| **View scale** | **Fit in Window** or **Actual Size**. See [Fitting the picture](while-streaming.md#fitting-the-picture). |

On Windows, the resolution and Windows' own text scaling are separate things. Moonwalk doesn't change Windows' text scaling.

### Keyboard and Mouse

**Automatic**, the default, uses the Automatic pointer below and draws the cursor on your computer. Choose **Manual** to set **Pointer** and **Local cursor** yourself; as with Display, your Manual choices are kept for later. **Type with this computer's keyboard layout** stays in view either way, because only you know whether your layout matches the host's.

**Pointer** decides how your mouse moves on the host:

| Choice | What it does | Best for |
| --- | --- | --- |
| **Automatic** | Follows your pointer on desktops, and switches to game-style movement in games | Almost everyone |
| **Desktop** | The host's pointer always sits exactly where yours is | Desktop work |
| **Game** | Sends how far the mouse moved, so games can turn the camera | Games that control the camera with the mouse |

**Type with this computer's keyboard layout** sends the text you type rather than physical key presses, so accents and other languages come out right. Shortcuts still use physical keys. Turn it off for games.

**Local cursor** draws the pointer on your computer, so moving it doesn't wait on the network. It's on for every new host. Moonwalk Host hides its own pointer while yours is shown. Sunshine, Apollo and their forks keep drawing theirs; press Ctrl+Alt+Shift+N during the stream to hide it there. That toggle stays switched on the host until you press it again.

### Advanced

| Setting | What it does |
| --- | --- |
| **Saved Settings** | Keeps named setups for this host so you can switch between them. |
| **Re-evaluate This Host** | Clears learned performance limits while preserving pairing, Manual choices, presets and last-working settings. |
| **Restore Last Working Settings** | Restores the settings recorded from a previous healthy stream. |

Decoder and frame-pacing controls described in older guides are not exposed in this adaptive settings page. The codec choice is under **Stream → Manual**. Audio behavior depends on the host and available playback devices; the current page has no per-host Channels selector.

### The host menu

The **⋮** button next to a host (the Host Actions button on KDE) has actions for that host, including **Host Details**. There you can see its information, copy any value, and choose a specific address to connect to. Leave the address on **Automatic** unless Moonwalk is picking the wrong network route. An address is only saved when you choose **Save** or **Automatic** (**Use Automatic** on KDE).

## In Preferences

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/preferences-dark.png">
    <img src="../assets/preferences-light.png" width="620" alt="The Preferences dialog, with sections for Streaming, Audio and Keyboard, each holding switches and dropdowns.">
  </picture>
</p>

A few settings people most often look for:

| Setting | What it does |
| --- | --- |
| **Streaming display mode** | Fullscreen is usually fastest. Borderless windowed works better with Alt+Tab, screenshots and overlays. |
| **End games when you disconnect** | Ends a streamed game or app; unsaved work may be lost, so this is off by default. Desktop streams request session cleanup in the background without logging out of the remote desktop; cleanup failures are reported. |
| **Mute the host's speakers while streaming** | Keeps the sound from coming out of both computers. Takes effect the next time an app starts on the host. |
| **Capture system keyboard shortcuts** | Sends shortcuts like Alt+Tab to the host instead of your own desktop. |
| **Find hosts on the local network automatically** | Turn this off if you'd rather add hosts by address. |
| **Language** | Moonwalk's display language. |

---

Next: [While you're streaming](while-streaming.md) · [Smoother streaming](performance.md) · [All guides](README.md)
