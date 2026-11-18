# Settings ownership: Starlight (client) ↔ Skylight (host)

This is the reference for *who owns which setting* across the Starlight/Skylight pair, so the two
apps don't grow duplicated, conflicting, or confusingly-split controls. It also records what was
deliberately deferred, with the reasoning, so those decisions aren't re-litigated or lost.

## The owner's doctrine (2026-07-18, verbatim intent)

> The host should always offer the maximum capabilities it can offer. The client is the machine
> that needs specifics — the host exposes all the features available; the client uses them.

Practical consequences: the host never grows constraint knobs (no `max_bitrate` control — the host
ships unclamped and stays that way); capability toggles on the host exist only where they are
genuine machine/egress policy (clipboard sync, virtual display), not quality/preference choices.
Every quality or preference decision (resolution, fps, bitrate, codec, cursor) lives on the client,
per host.

## The one idea that resolves most "duplication" worries

A client setting and a host setting sit at opposite ends of one pipe and answer **different
questions**:

- **Client (Starlight)** expresses *a user's preference for this session*.
- **Host (Skylight)** expresses *a machine capability and an operator policy*.

One host serves many clients; one client talks to many hosts (Skylight, stock Sunshine, Apollo,
GFE). So neither side can own a shared value alone. The genuine overlaps are therefore **request /
clamp pairs, not duplicates** — and both halves must exist:

| Concern | Client owns | Host owns |
|---|---|---|
| Bitrate | requests a target | clamps at `max_bitrate` |
| Codec | picks within what's offered | advertises support (`ServerCodecModeSupport`) |
| Resolution / fps | requests (now defaults to native, decode-guarded) | virtual display honors it |
| Cursor | declares whether it draws its own (`clientSideCursor`) | composites iff the client doesn't |

Look-alikes that are actually **disjoint**: the client's *video decoder* selection (decode) vs the
host's *encoder* selection (encode) — same vocabulary, different machines.

## Host-awareness

Starlight learns the host implementation from the optional `<HostOs>` serverinfo tag
(`serverSoftware` == `"Skylight"` / `"Sunshine"` / `"GeForce Experience"`). This gates Starlight-
specific defaults **only for Skylight hosts**, so stock hosts are never handed behavior they can't
coordinate:

- **Native-resolution default** (any host, decode-guarded): first-time hosts default to the
  client's native resolution, capped to 1080p when the client lacks hardware HEVC/AV1 decode.
- **Instant-cursor + remote-desktop-mouse default** (Skylight only): safe because the
  `clientSideCursor` handshake lets a Skylight host coordinate. Stock hosts keep the upstream
  (off) defaults, because a client-drawn cursor against a host that composites regardless is a
  double cursor.

Compatibility rule: every wire addition is an **additive, optional** serverinfo tag or query param
(the ecosystem convention — matches Apollo/Artemis prior art). Stock Moonlight clients and stock
hosts must keep working unchanged; absent-param behavior is always the legacy default.

## Dead / hidden settings (host, macOS)

- `qp` (Quantization Parameter) — parsed but read by no encoder path; hidden on macOS.
- `dd_*` display-device family — inert on macOS; the real mechanism is the on-demand virtual
  display sized to the client request.

## Owner rulings (2026-07-18) — decided, not pending

These items left the backlog by explicit owner decision on 2026-07-18. Do not re-flag them.

- **Native app-list editor (Skylight): DECLINED.** The owner does not use the apps list ("the
  whole apps thing is totally useless for its purpose" — he streams the desktop). App editing
  stays web-UI-only, permanently.
- **Web UI: stays as a hidden escape hatch, off by default.** With the apps editor declined, the
  web page keeps its one real job (app-list editing) buried behind the Advanced toggle. No full
  removal; upstream compatibility preserved.
- **Flathub / public repos: DECLINED for now.** Repos stay private; install stays via the remote
  build on the owner's client machine. This also parks every public-release-only item (WiX GUID
  mint, appdata screenshots, packaging version-stamp repoint) until the posture changes.
- **Capability tags + adaptive client UI: ratified DEFERRED.** No concrete client-side consumer
  needs feature-value gating yet — `serverSoftware`-gating covers today's cases and clipboard sync
  probe-and-latches. Adopt only when a real consumer appears.
- **HDR / surround auto-enable: ratified DEFERRED (conservative is correct).** Auto-HDR washes out
  SDR on marginal panels; channel-count signals lie. Moot on the current Haswell client.
- **Visible branding: SWEPT (both repos).** Skylight issue-URL default + hostname-fallback string
  repointed; Starlight was already clean (links, strings) except the `--help` binary-name bug,
  fixed. New Liquid-Glass icon family shipped: host = glass display with waves radiating out,
  client = the mirror, waves arriving. Identity strings untouched, as always.

## Resolved since first writing

- **Anchored pairing panel** (Skylight): superseded and DONE — pairing is now a sheet on the
  Settings window (see `Skylight docs/design/2026-07-18-pairing-attached-to-settings.md`),
  shipped and owner-verified working 2026-07-18. The standalone pairing window is retired.

## Kill list (deliberately never doing)

Bitrate double-adjustment math (correct, just opaque); the HEVC/AV1 hard-reject (only reachable via
a forced-codec override, where erroring is the contract); deleting dead config keys (hide only —
upstream-mergeable); moving clipboard/virtual-display *enable* to the client (host egress policy);
any fork-identity-string change (pairing/TCC depend on them); mDNS TXT capability records.
