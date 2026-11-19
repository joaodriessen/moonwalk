# Streaming performance

Press **Ctrl+Alt+Shift+S** while streaming to see the video statistics. For a
quick check, open **Stream Controls** with **Ctrl+Alt+Shift+O**.

When a moving picture looks uneven, compare these numbers while scrolling or
moving a window. A still desktop may send fewer frames.

| Statistic | What to look for |
| --- | --- |
| Requested frame rate | The target shown in Stream Controls. |
| Frames submitted for decoding | How much video reached the decoder. Look at packet loss and host processing if this falls well below the target during motion. |
| Rendered frames | How often Moonwalk draws the stream. Decode time, the display queue, and frames skipped to catch up help explain a drop here. |
| RTT | The network round trip. Input-to-picture delay also includes capture, encoding, decoding, and display time. |

Frames can go missing before decoding when the network drops packets or the
client's decode queue fills. The frame gap count alone can't tell you which.

## If the stream stutters

1. Start in **Automatic**. The host page shows the planned picture and rate.
   Check the statistics during motion to see what the stream delivers.
2. Try a wired connection if one is available. Wi-Fi can delay packets even
   when an ordinary speed test looks fine.
3. Check [hardware decoding](troubleshooting.md#is-hardware-video-decoding-working)
   on the client. A host can be encoding in hardware while its capture rate,
   the network, or the client's presentation is still limiting the stream.

A smaller stream may help when one of those stages is overloaded. If you try
that manually, keep the frame rate the same so you can tell which change helped.

## What Automatic changes

Moonwalk starts with the local display's resolution and refresh rate, subject
to known limits. It considers lowering bitrate before recommending a smaller
picture and keeps the requested frame rate during those size trials. A bitrate
target is a video encoder setting. Actual traffic varies with the picture and
includes transport overhead.

- **Automatic** bitrate can fall under sustained network pressure and rise
  again as the link recovers. On hosts with live bitrate control, Moonwalk can
  also try a lower target when the host itself is sending too few frames.
  Moonwalk is more cautious when a change may restart the encoder.
- **Recommended** starts with an estimate when live bitrate adjustment is
  unavailable. If delivery slows under sustained network or encoder pressure,
  Moonwalk can try a lower bitrate at the same resolution the next time you
  connect. A manual picture size and frame rate can still use this bitrate
  mode. Moonwalk keeps the lower starting rate if delivery or latency improves.
- **Limited** uses the target you chose under **Stream → Manual → Limit bandwidth**.

Moonwalk keeps a working stream connected. When it has enough evidence to
suggest a different picture, it queues the change for the next connection.
**Apply Now** in Stream Controls reconnects at your request. Moonwalk compares
a new picture with the previous baseline. If the change doesn't improve
delivery or latency, it queues a return to the previous picture. An actual
connection failure can trigger recovery using the current picture.

Learned results depend on the host, route, codec, and viewing display. They
expire and can be cleared under **Advanced → Re-evaluate This Host**. Use that
after a hardware or network change if old results are getting in the way.

## If text looks the wrong size

Host text size, encoded resolution, and how the picture fits the local window
are different settings. **Fit in Window** and **Actual Size** change the local
view. They don't change how the host lays out its desktop. On Windows, desktop
scaling is controlled by the host. See [Display settings](settings.md#display)
and [which desktop you get](../compatibility.md#which-desktop-you-get).

## Reporting a problem

Include the host and client versions, the network type, the requested mode, and
the incoming and rendered frame rates during motion. Note when the problem
starts, for example only after maximizing a window or when another device is
using Wi-Fi. Remove private information before sharing diagnostics.
