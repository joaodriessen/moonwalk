# While you're streaming

Move your pointer to the handle at the top of the picture and click it to
show the quick controls. Narrow windows have fewer buttons. If the pointer is
captured, press **Ctrl+Alt+Shift+Z** first.

Open **Stream Controls** with **Ctrl+Alt+Shift+O** or the Controls button.
The stream keeps playing while input stays with the controls. Press **Esc**
to return. The panel shows connection statistics, video encryption status,
and the options available for this connection.

## Keyboard shortcuts

Hold **Ctrl+Alt+Shift** and press:

| Key | Action |
| :---: | --- |
| **O** | Open Stream Controls |
| **Q** | Disconnect |
| **Z** | Release or recapture keyboard and mouse |
| **X** | Toggle fullscreen |
| **K** | Toggle capture of system shortcuts |
| **S** | Show or hide performance statistics |
| **V** | Type the local text clipboard on the host |

Open **Keyboard Shortcuts** from Moonwalk's main menu to see the list in the
app.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/shortcuts-dark.png">
    <img src="../assets/shortcuts-light.png" width="620" alt="The Keyboard Shortcuts dialog with keys and actions for a stream.">
  </picture>
</p>

## Fitting the picture

**Fit in Window** shows the whole remote screen. **Actual Size** draws one
remote pixel per local pixel. If the picture is larger than the window,
release input and scroll to move around it. Click the picture to control the
host again. Both options leave the host's display alone.

The stream controls follow your desktop's light or dark appearance. Your
desktop manages the window size on tiling compositors such as Hyprland.

## Changing quality during a stream

If the host supports bitrate changes, Stream Controls shows a slider. Some
encoders pause briefly to apply a change. If the host refuses the request, the
slider returns to the last confirmed value. The number shown is a target in
Mbps. Actual traffic depends on the picture.

Moonwalk can recommend a different resolution after measuring a stream. It
queues the change for the next connection. Choose **Apply Now** to reconnect
right away. See [Streaming performance](performance.md).

## Copy and paste

Stream Controls offers clipboard directions when the host supports them.
Choose both directions, one direction, or **Paused**.

| Host | What you can copy |
| --- | --- |
| **Moonwalk Host** | Text and images |
| **Apollo and Vibepollo** | Text, if the host allows it for this device |
| **Other hosts** | Use **Ctrl+Alt+Shift+V** to type local clipboard text on the host |

Image clipboard sync uses PNG, with limits of 4 MiB and 4096 pixels per side.
Larger images leave the clipboard as it was.

On Wayland, your desktop may require an interaction before accepting a
clipboard change. Click the stream before copying. If paste stops working just
after disconnect, click the Moonwalk window and try again.

## Copying files

Moonwalk Host supports files up to 8 GiB each, in both directions.

- To send a file, drag it into the stream or the Files section, or use
  **Choose Files…**. Check the destination shown in the panel.
- To receive a file, browse the host's Downloads, Documents, or Desktop
  folder, select a file and a local destination, and press **Receive File**.
  Use **Open Received Folder** to find it afterward.

If a file already exists, choose **Keep Both** or **Skip** when sending. When
receiving, choose another name or explicitly allow replacement. Folders
can't be sent as a group. Transfers continue while you use the remote desktop
and cancel when you disconnect. macOS permissions may restrict which folders
Moonwalk Host can read or write.

## Disconnect and recovery

**Disconnect** closes local video and input and returns to the host list.
Moonwalk asks the host to end the Desktop stream in the background without
logging out of the remote desktop. If Moonwalk launched a separate game or app,
**End games when you disconnect** in Preferences controls whether it asks the
host to quit that app. Save your work before using that option.

If the connection drops, Moonwalk can try up to five times within a minute to
resume the stream. Cancel at any time. Cancelling also requests cleanup of the
Desktop held for recovery. Moonwalk reports when the host can't confirm
cleanup. An offline host may keep the target resumable until it can be reached.

After a failed start, Moonwalk can offer:

- **Try Last Working Settings** to connect using the last healthy settings
  without replacing your saved choices.
- **Retry with Safe Settings** to try 1920×1080 at 60 FPS and 10 Mbps, with SDR,
  stereo, and automatic codec and decoder selection. Keep them explicitly if
  you want them as this host's saved settings.

[Streaming performance](performance.md) · [Troubleshooting](troubleshooting.md) · [All guides](README.md)
