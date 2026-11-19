# Testing stream recovery

Run `scripts/validation/stream-recovery.py` in a real graphical/audio session against an already paired test host. Supply an existing **private copy** of the client's XDG config directory; never select `~/.config` or `~/.var/app`. The runner refuses those production locations and symlinks. Each trial gets a separate copy under its new output directory, with private config/data/cache/state. The supplied copy stays unchanged. Display, audio and runtime session environment remain available for the real stream.

```sh
python3 scripts/validation/stream-recovery.py \
  --host TEST-HOST --app Desktop \
  --config-home /absolute/private-profile/config \
  --output /absolute/private-evidence/new-run \
  --cycles 2 --seconds 15 --startup-budget 60 --total-budget 240 \
  -- /absolute/candidate/starlight
```

The client prefix after `--` is an argv list, never a shell command. For Flatpak, use `-- flatpak run --command=starlight com.starlight.Starlight`; the runner adds a temporary filesystem grant and invokes `env` inside the sandbox to set its trial config/data/cache/state paths before the original client executable. Flatpak resets reserved XDG variables after `--env` processing, so those flags alone do not isolate a profile. The outer Flatpak launcher keeps the caller's XDG paths to find its user installation. The runner signals only its owned `starlight` descendant. Accepted Flatpak prefixes end with APPID, include explicit `--command=CLIENT` (or `--command CLIENT`), and use `--option=value` for other options with values; flag options are preserved. Extra arguments after APPID, ambiguous separated option values, missing/empty commands and executable names containing `=` are rejected before launch. Paths and client arguments remain individual argv entries; no shell interprets them. No installation or Flatpak settings change occurs. Do not use a launcher that forwards to an already-running client. Additional stream options use repeated arguments, for example `--stream-arg=--1080 --stream-arg=--fps --stream-arg=60`. The runner appends `--no-quit-after` to keep launched applications open. Moonwalk still requests its normal Desktop session cleanup. Streaming can wake the display or move the remote pointer.

Each default trial connects, holds the stream, and sends SIGTERM to the test client so it exits normally. The next trial reconnects. `--abrupt` adds a connected trial killed with SIGKILL followed by a mandatory graceful reconnect. This deliberately interrupts only the test's own client. All trials have startup, hold, shutdown and total deadlines; timeout or interruption kills/reaps only owned descendants, including detached sandbox children. A Linux child subreaper belongs to the runner process, with no service or system setting changes. No `pkill`, `flatpak kill`, host/network command or profile maintenance occurs. Output contains private client logs and each trial's disposable profile copy; keep it private.

Use `--after-abrupt-delay SECONDS` with `--abrupt` to wait for the host's old
transport to expire before measuring the reconnect. The delay counts against
the total budget. An immediate reconnect can overlap the crashed client's old
session on a host that permits multiple viewers. Comparing incoming and
rendered frame rates helps identify where cadence dropped.

Each `.log` is combined client stdout/stderr. `summary.json` records argv, timing, signals, exit status, cleanup and parsed facts. Exit zero requires every scheduled trial to pass. Missing startup/frame evidence, unexpected early/nonzero exit, log overflow, surviving owned processes or a deadline fails the run. A successful connection requires the real `Starting input stream...`/following SDL `done` and `Received first video packet` messages. Every graceful trial must also emit a `Global video stats` summary with positive `Rendering frame rate`. Packet receipt alone cannot prove rendering. SIGKILL cannot flush that summary: the abrupt seed explicitly leaves rendering unverified and the subsequent graceful reconnect must prove it.

Compare the frame rates submitted for decoding, decoded and rendered. When
the renderer supplies presentation feedback, the report includes the time
from decoding to display and the percentage of frames replaced before they
reached the screen. Frames skipped to catch up with the display have their
own metric. Historical logs use the older network jitter label.
The parser also records network RTT, decode time, queue delay, rendering
time and host processing latency when present. It records frame gaps before decoding separately. Older logs can
label the same gaps as network drops. RTT is the network round trip, and
rendering time includes the wait for V-sync. Measuring physical display and
speaker latency or checking the picture and sound requires a separate test.
The renderer must produce global FFmpeg statistics for this check to pass.

Sleep/wake, audio-device changes, display changes, clipboard focus and physical input are explicitly recorded as **not executed**. Exercise them separately in a controlled session and record the exact event, observed recovery and relevant log range. A pass covers the measured disconnect and reconnect trials on that client and host. Record candidate source/artifact hashes and host version beside the output before using it as release evidence.

Parser/owned-process checks run without streams, network or credentials:

```sh
python3 -m unittest discover -s tests/tools -p test_stream_recovery.py -v
```
