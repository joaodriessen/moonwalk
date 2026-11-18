# Smoother streaming

A stream has three stages, and any of them can cause a stutter or delay: **the host** captures and compresses the screen, **the network** carries it, and **your computer** decompresses and shows it.

## What to try, in order

1. **Leave Stream on Automatic.** Moonwalk targets your local display, considers lower bitrate before smaller pictures, and preserves the requested frame rate during size trials. It never reconnects a working stream to tune performance. It cannot make every host sustain every display mode.
2. **Check the connection.** Ethernet is useful for distinguishing network problems from host or client limits. Wi-Fi can lose or delay packets even when its average speed is high.
3. **Check video decoding and the stream statistics.** Hardware encoding on the host and hardware decoding on your computer are separate. Neither alone proves a smooth stream. See [hardware video decoding](troubleshooting.md#is-hardware-video-decoding-working).
4. **Report persistent stutter.** Include whether the scene was moving, the actual received/rendered frame rates, and any reconnects or pauses. Lowering a setting does not by itself identify the bottleneck.

Manual stream settings remain available if you want a deliberate override. A lower frame rate reduces motion smoothness; it is not something Automatic silently chooses to hide a struggling host.

## Smoothness and delay

Automatic pacing can briefly hold frames to avoid replacing them before the screen shows them. That trades some delay for steadier motion when arrivals bunch up. The statistics expose this tradeoff. Queuing, decode time and host processing time are separate measurements; none alone measures the delay from a key press to its visible result.

## See what's happening

Press **Ctrl+Alt+Shift+S** during a stream to show frame rate and timing statistics, including how many frames never reached your screen and how long frames take to get there. Stream Controls (**Ctrl+Alt+Shift+O**) also shows the round trip, frame rate and whether the connection is healthy.

## About bitrate settings

- **Automatic** is the one to use. It watches frame delivery and can lower the bitrate when sustained measurements show pressure. When there's room again, it climbs back. It remembers recent results for the same host build, codec, stream mode and route, so the next stream can start close to them. The bar shows the current encoder target; actual traffic depends on the picture.
- **Recommended** appears instead on hosts that can't change bitrate during a stream. It picks a sensible amount for your resolution and frame rate, and stays there.
- **Limited** reflects **Stream → Manual → Limit bandwidth**. It uses your chosen target; actual traffic and transport overhead differ from that target.

On Moonwalk Host, Automatic adjusts the Mac's encoder directly. On Windows hosts with live bitrate control, each change can restart the host's encoder, so automatic targets are deferred unless that path has demonstrated live changes.

## What Automatic learns

Automatic starts at the local display's resolution and refresh rate. Processing delay alone does not prove a host is short of frame rate, and a static desktop sending fewer updates does not demonstrate overload. Moonwalk measures delivered cadence and active picture data, supported by capture telemetry when available.

A working stream stays connected. Supported quality controls adjust live; a host that might pause or rebuild its encoder is not probed automatically. Its estimated quality target is remembered for the next connection. Stream Controls shows the applied target and explains when changes are deferred.

Sustained poor delivery can suggest one smaller picture, preserving frame rate. A long healthy stretch can suggest a larger picture. These suggestions apply on the next normal connection. They also appear quietly in Stream Controls with **Apply Now**, which explicitly reconnects; there are no countdowns or automatic acceptance. Recovery from a real connection failure preserves the current picture.

A changed picture is checked against an independent baseline. An ineffective reduction or failed detail recovery queues the previous picture for the next connection, with the same optional Apply Now action. It never interrupts video to undo a trial. Failed measurements do not replace the baseline. A supported smaller picture must improve delivery or latency to be retained.

Suggestions belong to the host, build, route, viewing display and codec, and expire after a day. They are not universal hardware ratings. Remembered capacity also expires. A recovered connection can withdraw an unnecessary reduction. **Re-evaluate This Host** in Advanced settings clears learned performance while preserving pairing, Manual choices, presets and last-working settings.

On Sunshine-family hosts without desktop-scale negotiation, encoded-size adjustments keep the host desktop's geometry fixed. Hosts with scale negotiation use supported scales at or above the local logical desktop. Initial scale matching still depends on host capabilities and display policy. The FFmpeg decoder checks actual decoded dimensions; other paths currently use negotiated dimensions. An ignored size request stops further trials.

Round trip, host processing, decoding, frame queuing and presentation measure different parts of the pipeline. They are not interchangeable and should not be added together as an input-to-screen latency measurement.

## Reporting a performance problem

Please include the host software, whether you're on Wi-Fi or a cable, your video settings, and what the statistics showed. That usually narrows it down quickly.

---

Next: [Troubleshooting](troubleshooting.md) · [All guides](README.md)
