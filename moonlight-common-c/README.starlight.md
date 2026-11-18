# Starlight receiver feedback extension

The nested `moonlight-common-c` repository remains the public upstream submodule,
pinned to `2ea47752c3051d72a64bcca190024e8b354fa1ef`. Do not create or push a
private replacement commit into its gitlink for this extension.

The outer qmake target compiles `AdaptiveVideoQueue.c` instead of upstream
`src/RtpVideoQueue.c`. This tracked wrapper includes that original file, renames
its init/add/cleanup entry points, and forwards packet processing unchanged.
There is no generated source copy, Python build dependency or upstream edit.
`adaptivefeedback.h` is a Starlight-only connection setup API; it does not change
upstream's public API or stream-configuration structure.

For a successful launch/resume with fresh host capability and explicit adaptive
opt-in, Session enables observations immediately before `LiStartConnection`.
Cleanup revokes the opt-in. Default, unsupported-host and disabled sessions retain
upstream reporting. The receive thread observes real completion across every FEC
block and offers at most four clean-frame reports per second through upstream's
existing bounded queue, control serializer and encryption. It never infers health
from a timer, silence or decoder progress. Partial, rejected, duplicate and late
packets do not create reports; a frame needing any FEC recovery cannot be clean.
The pinned upstream missing-block path can report stale full-block counters: in
adaptive mode clean-looking upstream reports are suppressed unless they came from
our witnessed complete-frame path. Actual loss reports retain the upstream path.

`AdaptiveControlStream.c` wraps upstream `src/ControlStream.c` the same way and adds
`StarlightSendControlMessage` and a read-only `StarlightGetEncryptionFlags` snapshot
of the active negotiated audio/video protection. These use the pinned internal
control/connection state; an upstream update must review those flags too. A session using Automatic bitrate also sets a frame
observer: the receive thread reports when each frame's packets arrived, how much
video data came and how much went missing, before upstream sees the packet. Upstream
packet handling is unchanged, and with no observer set nothing extra runs.

## Maintenance coupling

This wrapper intentionally depends on the pinned queue's ownership/completion
contract: submission can free the input packet; completed-block counters remain
available after the queue advances; only the final block advances the frame.
The wrapper snapshots input metadata before that ownership transfer. An upstream
pin update must review those assumptions and the missing-block report behavior,
update the source hash comment, and run the registered `adaptivefeedback` suite.
Do not silently replace its completion checks with frame-number advancement alone.

The ordinary hosted suite checks actual receiver/FEC processing, bounded queueing,
AES-GCM wire packets, lifecycle, disabled/other-host preservation and missing-block
handling. Its socket/decode boundaries are inert; time is virtual. A private
integration run may supply `ADAPTIVE_HOST_SOURCE=/path/to/skylight` to qmake to feed
those actual wire observations into the host's real parser/controller and verify
loss, silence and recovery. Ordinary client builds/tests have no host-repository
dependency. This fixture is not a substitute for a later coordinated paired
network-capacity reduction/recovery and visual-disruption check.
