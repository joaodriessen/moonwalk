# Per-host stream settings for Starlight

Design only. No implementation in this document. Author: architect pass, 2026-07-14.
Project: Moonwalk, based on Moonlight Qt.

## Problem

`StreamingPreferences` is a single global object (`app/settings/streamingpreferences.{h,cpp}`),
registered as the QML singleton `StreamingPreferences 1.0` (`app/main.cpp:966`) and persisted to
one flat `QSettings` store. Every connection parameter (resolution/fps/bitrate/vsync/pacing/
codec/decoder/yuv444/hdr/audio) is shared across all hosts. The owner wants connection settings
to be genuinely per-host (Mac mini at native/60/high; a Windows box at something else) while
app-behavior settings stay global.

## Evidence base (what the code actually does today)

- **Singleton + QML binding.** `StreamingPreferences::get()` returns a process-global
  `s_GlobalPrefs` (streamingpreferences.cpp:61,71-106). QML binds every control directly to this
  one singleton (`app/main.cpp:966-970`). PcView.qml binds resolution/fps/bitrate/vsync/pacing/
  localCursor/perf-overlay straight to `StreamingPreferences.<prop>` (PcView.qml:159-190, 722,
  877, 907-913, 973-1006, 1031-1034).
- **Prefs consumed at launch via the same singleton.** `Session::Session(NvComputer*, NvApp&,
  StreamingPreferences* = nullptr)` falls back to `StreamingPreferences::get()` when no prefs are
  passed (session.cpp:552-553). All GUI session builders pass no prefs, so they use the global
  singleton: `AppModel::createSessionForApp` (appmodel.cpp:47), `ComputerModel::
  createSessionForCurrentGame` (computermodel.cpp:144), `ComputerModel::createSessionForDesktop`
  (computermodel.cpp:181). Session reads `m_Preferences->*` throughout `initialize()`/decoder
  selection on the streaming thread (session.cpp:619, 804, 993-1249).
- **CLI path is the one explicit-prefs caller.** `main.cpp:1007-1014` parses CLI options into the
  global prefs, then `startstream.cpp:105` builds `new Session(m_Computer, app, m_Preferences)`.
  The CLI parser mutates prefs fields directly and only when a flag is present
  (commandlineparser.cpp:393-407).
- **Host object + stable key.** `NvComputer::uuid` is a persisted field (nvcomputer.h:143,
  serialized as `SER_UUID="uuid"`, nvcomputer.cpp:94) and is already the map key for
  `m_KnownHosts` / `m_PollEntries` / `m_LastSerializedHosts` in ComputerManager
  (computermanager.cpp:183, 404, 467, 863, 921, 958). It is the correct per-host key. Confirmed
  stable.
- **How hosts persist.** ComputerManager stores hosts as a QSettings *array*
  `beginWriteArray(SER_HOSTS="hosts")` plus a `hostsbackup` array (computermanager.cpp:14-15,
  281-309). Each save does `settings.remove("hosts")` then rewrites the whole array
  (computermanager.cpp:296-303). Individual host fields are written by `NvComputer::serialize`
  (nvcomputer.cpp:92-105). Global prefs live at the QSettings top level as flat keys
  (`width`, `fps`, ... streamingpreferences.cpp:13-55).
- **Existing migration precedent.** `starlightPrefsVersion` marker in `reload()`
  (streamingpreferences.cpp:55, 219-227): idempotent, versioned, independent of upstream's
  `SER_DEFAULTVER`/`CURRENT_DEFAULT_VER`, writes its marker immediately so it runs exactly once.
  This is the pattern to reuse.
- **Selection state today.** PcView drives everything off `sidebarList.currentIndex` /
  `currentItem.hostIndex`; `contentColumn.cur` is the selected delegate (PcView.qml:359-403, 639,
  690, 735). During an active stream the StreamSegue is pushed on top of the stackView
  (PcView.qml:300-308), so PcView is not interactable and the selection cannot change mid-stream.
- **SettingsView is the only UI exposing several connection settings.** PcView does NOT show
  codec/decoder/HDR/YUV444/audio-channel/absolute-mouse/unlock-bitrate; SettingsView.qml does
  (PcView.qml:23-29 says so explicitly). This is load-bearing for the UI plan below.

## 1. Taxonomy — explicit per-property assignment

Every `StreamingPreferences` member, its SER key, and its scope. "PER-HOST" = keyed by
`NvComputer.uuid`; "GLOBAL" = stays a flat top-level key exactly as today.

| Property | SER key | Scope | Rationale |
|---|---|---|---|
| width | width | PER-HOST | Owner-approved. Resolution is a per-host connection param. |
| height | height | PER-HOST | Owner-approved. |
| fps | fps | PER-HOST | Owner-approved. |
| bitrateKbps | bitrate | PER-HOST | Owner-approved. |
| unlockBitrate | unlockbitrate | PER-HOST | Owner-approved. Bitrate cap belongs with bitrate. |
| autoAdjustBitrate | autoadjustbitrate | PER-HOST | Not in owner list. Governs whether bitrate is auto-derived from this host's res/fps; presets flip it per host (PcView.qml:162,171,180). Must travel with bitrate or presets desync. |
| enableVsync | vsync | PER-HOST | Owner-approved. |
| framePacing | framepacing | PER-HOST | Owner-approved. |
| localCursor | localcursor | PER-HOST | Owner-approved. Pairs with a host-side capture_cursor setting. |
| absoluteMouseMode | mouseacceleration | PER-HOST | Owner-approved ("optimize mouse for RDP"). |
| audioConfig | audiocfg | PER-HOST | Owner-approved (channel layout). |
| videoCodecConfig | videocfg | PER-HOST | Owner-approved (codec mode). |
| enableHdr | hdr | PER-HOST | Owner-approved. Host capability differs. |
| enableYUV444 | yuv444 | PER-HOST | Owner-approved. |
| videoDecoderSelection | videodec | PER-HOST | Owner-approved. |
| packetSize | packetsize | PER-HOST | Not in owner list, no UI, default 0 (auto). Transport param scoped to a network path → per-host is the conceptually correct home. Zero behavior change (all hosts seed to the same 0). Low stakes; global would also be defensible. |
| language | language | GLOBAL | Owner-approved. App chrome. |
| uiDisplayMode | uidisplaymode | GLOBAL | Owner-approved. |
| windowMode | windowmode | GLOBAL | Not in owner list. Governs the local client window (fullscreen/borderless/windowed), not the host connection; consistent local behavior is expected. Also feeds `recommendedFullScreenMode` fixups. GLOBAL. |
| recommendedFullScreenMode | (derived) | N/A | CONSTANT, computed in reload(), never persisted. |
| gameOptimizations | gameopts | GLOBAL | Owner-approved (optimizeGameSettings). |
| playAudioOnHost | hostaudio | GLOBAL | Owner-approved (mute-host-audio). |
| multiController | multicontroller | GLOBAL | Owner-approved (gamepad). |
| enableMdns | mdns | GLOBAL | Owner-approved. |
| quitAppAfter | quitAppAfter | GLOBAL | Owner-approved. |
| absoluteTouchMode | abstouchmode | GLOBAL | Owner-approved (touchscreen-trackpad). |
| connectionWarnings | connwarnings | GLOBAL | Owner-approved. |
| configurationWarnings | confwarnings | GLOBAL | Owner-approved. |
| richPresence | richpresence | GLOBAL | Owner-approved (Discord). |
| gamepadMouse | gamepadmouse | GLOBAL | Owner-approved (gamepad). |
| detectNetworkBlocking | detectnetblocking | GLOBAL | Owner-approved. |
| swapMouseButtons | swapmousebuttons | GLOBAL | Owner-approved. |
| muteOnFocusLoss | muteonfocusloss | GLOBAL | Owner-approved. |
| backgroundGamepad | backgroundgamepad | GLOBAL | Owner-approved (gamepad). |
| reverseScrollDirection | reversescroll | GLOBAL | Owner-approved. |
| swapFaceButtons | swapfacebuttons | GLOBAL | Owner-approved (gamepad). |
| keepAwake | keepawake | GLOBAL | Owner-approved. |
| captureSysKeysMode | capturesyskeys | GLOBAL | Owner-approved (capture-system-keys). |
| showPerformanceOverlay | showperfoverlay | GLOBAL (judgment) | Not in owner list. A diagnostic display toggle, not a connection parameter — nothing about it is host-specific. Recommend GLOBAL. Note: it currently renders on the PcView per-host "Advanced" section (PcView.qml:1030); either move it to the cog, or leave it rendered there but backed by the global store. Owner call. |
| defaultver / starlightPrefsVersion | defaultver / starlightPrefsVersion | GLOBAL (meta) | Migration bookkeeping. |

Quality presets (PcView.qml:154-191) operate only on the PER-HOST subset (width/height/fps/
bitrate/autoAdjust/vsync/framePacing) → presets are inherently per-host with this taxonomy.
`detectPreset()` (PcView.qml:115-138) reads the same subset → it re-derives correctly per host.

## 2. Storage model

**Per-host connection settings: a QSettings group `hostsettings/<uuid>/<serkey>`, reusing the
existing SER_ key strings.**

- Keyed by `NvComputer.uuid`, e.g. `hostsettings/{a1b2...}/width = 3840`.
- **Coexistence.** The hosts *array* lives under the `hosts` / `hostsbackup` groups; Computer
  manager only ever does `remove("hosts")`/`remove("hostsbackup")` (computermanager.cpp:296,309).
  Global prefs are flat top-level keys. A distinct `hostsettings/*` prefix collides with none of
  them. HARD CONSTRAINT: the group must not be named `hosts`.
- **Survives host add/remove.** Because it is a group keyed by uuid (not an index array), adding
  or deleting a host never shifts other hosts' blocks. The hosts-array rewrite never touches
  `hostsettings`.
- **No corruption of the global store.** Global keys are never removed or rewritten by the
  per-host layer; the per-host layer only ever reads/writes under `hostsettings/<uuid>/`.
- Same SER_ constants are reused for both scopes, so read/write code for a key is shared and
  there is one source of truth for the key string.

Rejected alternative: a single serialized `QVariantMap` blob per host inside `NvComputer::
serialize`. It couples connection prefs to the host-array rewrite cycle (delayed-flush worker
thread), muddies `isEqualSerialized`, and loses the free coexistence the separate group gives.
Kept the group.

## 3. Lifecycle / effective-value resolution

**Chosen mechanism: current-host context on the existing singleton — `StreamingPreferences::
setCurrentHost(const QString& uuid)` — using an overlay-over-template model.**

Model:
- The existing flat global connection keys become a **default template** for new hosts. `reload()`
  keeps loading them into the object's fields exactly as today (no change).
- `setCurrentHost(uuid)` re-reads each PER-HOST field as
  `settings.value("hostsettings/"+uuid+"/"+serkey, <current in-memory template value>)`, then
  emits every connection NOTIFY signal (displayModeChanged, bitrateChanged, enableVsyncChanged,
  framePacingChanged, localCursorChanged, audioConfigChanged, videoCodecConfigChanged,
  enableHdrChanged, enableYUV444Changed, videoDecoderSelectionChanged, absoluteMouseModeChanged,
  unlockBitrateChanged, autoAdjustBitrateChanged). App-scope fields are untouched.
- `save()` splits: PER-HOST subset → `hostsettings/<m_CurrentHostUuid>/<serkey>`; GLOBAL subset →
  flat keys as today. The template (flat connection keys) is NOT rewritten by save() post-feature,
  so it stays frozen as the new-host seed. If `m_CurrentHostUuid` is empty (no host selected /
  CLI-list), save() writes the connection subset to the flat template as a harmless fallback
  (== today's behavior).
- Effective value for host H, key K = `hostsettings[H][K]` if present, else template[K], else the
  hardcoded default already in `reload()`.

Effective mechanism justification — Option A (this) vs Option B (separate `PerHostPreferences`
object bound by QML for connection settings while `StreamingPreferences` keeps app settings):

- **Option A wins on restraint and blast radius.** PcView already binds every connection control
  to `StreamingPreferences.<prop>`. Option A keeps those bindings verbatim; reactivity comes free
  from the existing NOTIFY signals fired by `setCurrentHost`. Session's prefs-pointer path is
  unchanged (still one singleton). The CLI path (which mutates the global object) keeps working.
- **Option B** is conceptually cleaner (hard separation) but forces rewriting every PcView
  connection binding to a new object, threading that object through all `createSession*` callers
  and the Session ctor, and duplicating the QML-singleton registration and lifetime logic. That is
  a prefs re-architecture — explicitly out of scope per the restraint directive.
- Chosen: **Option A.** The split lives inside `save()`/`setCurrentHost()`; the object's public
  shape and all QML bindings are preserved.

QML reactivity: PcView must call `StreamingPreferences.setCurrentHost(uuid)` whenever the sidebar
selection changes. Firing the connection NOTIFY signals makes the chip (PcView.qml:722), preset
segmented control (796), resolution/fps labels (836/877), bitrate slider (907), and Feel toggles
(973-1006) re-evaluate automatically because they read those properties.

## 4. Stream-launch integration

Precedence: **CLI flag > selected-host stored value > template default.**

- **GUI (primary).** In each session builder — `AppModel::createSessionForApp`,
  `ComputerModel::createSessionForCurrentGame`, `ComputerModel::createSessionForDesktop` — call
  `StreamingPreferences::get()->setCurrentHost(computer->uuid)` immediately before
  `new Session(computer, app)`. This guarantees the singleton holds the launch host's connection
  values regardless of UI state, so Session (which reads the same singleton, session.cpp:553)
  streams with the right values. In practice the sidebar selection already pinned the same host,
  so this is belt-and-suspenders; keep it because it removes the implicit coupling and covers any
  future non-PcView launch path.
- **CLI.** `main.cpp:1007-1014` parses flags into the global prefs *before* the target computer is
  known (it's resolved async in startstream.cpp). To honor precedence: in startstream.cpp at the
  ComputerFound handler (where `m_Computer` is set, ~line 84), call
  `m_Preferences->setCurrentHost(m_Computer->uuid)` to load that host's stored connection base,
  then re-apply the CLI overrides on top. Because the CLI parser only sets fields when the flag is
  present (commandlineparser.cpp:393-407), the Launcher should record which keys were explicitly
  provided and re-apply just those after `setCurrentHost`. This is a smaller, well-scoped
  follow-up; the GUI path is the requirement's core and works with the builder-side call alone.
- Do NOT adopt a "mutate-global-then-launch without host context" hack and do NOT clone
  StreamingPreferences per launch — the overlay-on-singleton path already yields host-correct
  values with no new object lifetime to manage.

Hardening option (recommended, not required): have Session snapshot the PER-HOST connection fields
into `m_StreamConfig` at construction (it already copies most of them early). This makes an active
stream immune to any later `setCurrentHost`, closing the theoretical mid-stream race (§7).

## 5. Migration (must not disrupt the owner)

Extend the existing `starlightPrefsVersion` mechanism in `reload()`; bump the marker to 2. Runs
once, idempotent, independent of the app version string.

```
starlightPrefsVersion < 2:
  # global connection keys have just been loaded into the in-memory fields (the template)
  enumerate host uuids by reading the existing hosts array from the SAME QSettings:
    settings.beginReadArray("hostsbackup") (fallback to "hosts") -> collect uuid per index
  for each uuid:
    for each PER-HOST serkey:
      if NOT settings.contains("hostsettings/"+uuid+"/"+serkey):
        settings.setValue("hostsettings/"+uuid+"/"+serkey, <current template value for that key>)
  settings.setValue(starlightPrefsVersion, 2)
```

- Seeds EVERY existing host's block from the CURRENT global values, so the Mac mini keeps its live
  config unchanged (native/60/47.5 Mbps/vsync-on/pacing-off/localCursor). Because the values are
  the just-loaded template, the seed is exactly the owner's current state.
- Idempotent twice over: gated on the version marker AND a per-key `contains()` check, so a
  half-completed run or a re-run never clobbers a value the owner has since changed.
- New hosts discovered LATER are not enumerated at migration time; they simply have no block and
  fall through to the template default at `setCurrentHost` — no crash, sensible defaults.
- Enumeration reads the hosts array directly from QSettings (no dependency on a live
  ComputerManager instance), so this is safe to run inside `reload()` at construction.
- The pre-existing `starlightPrefsVersion < 1` block (vsync/pacing correction) is untouched and
  still runs for profiles that never saw it.

## 6. UI impact (high level, no code)

Target state (requirement 6): PcView host page = per-host connection settings, reactive to sidebar
selection; the cog (SettingsView) = global app settings only.

- **ComputerModel:** add a `uuid` role (alongside the existing name/online/paired roles,
  computermodel.cpp roleNames) so QML can read `model.uuid`.
- **PcView.qml:**
  - Expose `hostUuid: model.uuid` on the sidebar delegate (near PcView.qml:398).
  - On selection change (a handler on `sidebarList.currentIndex`/`currentItem`), call
    `StreamingPreferences.setCurrentHost(currentItem.hostUuid)`. Existing bindings then refresh via
    NOTIFY — minimal churn.
  - To reach the TARGET (connection settings fully leave the cog), PcView must GAIN controls for
    the connection settings only SettingsView exposes today: video codec mode, video decoder,
    HDR, YUV 4:4:4, audio channel layout, "optimize mouse for RDP" (absoluteMouseMode), and
    unlock-bitrate. These become new rows in the per-host "Video"/"Feel"/"Advanced" groups. This
    is the largest UI work item and needs owner visual sign-off (Starlight's owner-visual-signoff
    convention).
- **SettingsView.qml:** remove the connection rows listed in the PER-HOST table
  (resolution/fps/bitrate/unlock/autoadjust/vsync/framePacing/localCursor/absoluteMouse/audioCfg/
  codec/decoder/HDR/YUV444). What remains is the global set (language, UI display mode, warnings,
  rich presence, keep-awake, mDNS, host-audio, focus-loss mute, quit-after, game-optimize, gamepad
  options, mouse-button swap, reverse-scroll, touch-trackpad, capture-system-keys, network-block
  detect, and — owner's call — the perf overlay).
- **Phased safe rollout for the UI** (recommended): Step 1 ships the store/`setCurrentHost` wiring
  with NO control relocation. The cog's connection controls stay put but, because the singleton is
  now host-scoped, they edit the CURRENTLY-SELECTED host's connection settings. Slightly surprising
  but functionally correct and near-zero risk; nothing becomes unreachable. Step 2 relocates the
  controls into PcView and strips them from the cog (the owner-eyes visual change). This decouples
  the correctness-critical store change from the larger UI churn.

## 7. Edge cases

- **Host deleted:** orphan `hostsettings/<uuid>/*` block. Clean it up: in the host-deletion path
  (`ComputerManager::deleteHost` / `DeferredHostDeletionTask`, computermanager.cpp:540, ~505-522)
  add `settings.remove("hostsettings/"+uuid)`. Prevents unbounded cruft and stale-uuid reuse.
  Low effort, safe.
- **Host not yet paired / uuid not yet assigned:** a manually-added host may have an empty/
  placeholder uuid until serverinfo is fetched. `setCurrentHost("")` must be a graceful no-op that
  falls back to the template (guard on empty uuid). No crash.
- **Offline / unknown host selected in sidebar:** its stored block still loads by uuid; the page
  shows stored values even when unreachable. Fine.
- **Desktop-only streaming:** same `createSessionForDesktop` path → `setCurrentHost(computer->uuid)`
  applies. Fine.
- **Concurrent access:** `setCurrentHost`/`save` run on the UI thread; Session reads fields on the
  connection thread. Two facts make this safe today without new locking: (a) only one Session is
  ever active, and (b) PcView is covered by the pushed StreamSegue during a stream, so the
  selection — and thus `setCurrentHost` — cannot fire mid-stream. QSettings serializes writes to a
  given file internally, so the per-host `save()` and ComputerManager's delayed-flush worker
  writing different groups is safe (same as today's global save vs hosts flush). The recommended
  hardening is the Session snapshot-at-construction (§4), which removes the residual assumption
  entirely. No `QReadWriteLock` on the prefs fields is warranted.
- **New host after migration:** no block → template fallback (§5). No crash.

## 8. Rollout

- **Straight rollout, no runtime feature flag.** The migration is versioned + doubly idempotent,
  and the overlay-over-template fallback means the feature is inert until `setCurrentHost` is
  actually called (e.g. `moonlight --list` never calls it → behaves exactly as today).
- **Downgrade safety / no data loss.** The global flat connection keys are LEFT INTACT and are
  never deleted; an older build without per-host support reads them and works. The `hostsettings/*`
  group is simply ignored by old builds. Caveat to document: after the owner edits per-host values
  on a new build, the frozen template reflects the pre-upgrade values, so a downgrade would show
  those (last template) rather than the newest per-host edits — stale, not broken, no loss.
- **Version note.** `--version` becomes "Starlight 0.1.0"; nothing here keys off the app version
  string. Migration keys off the independent `starlightPrefsVersion` marker only. Do not couple
  the migration to `SystemProperties.versionString` or to `CURRENT_DEFAULT_VER`.

## Prioritized recommendations (improvement x safety / effort)

SAFE-AUTONOMOUS (correctness-critical store change; ship behind the version marker):
1. **Storage group + `setCurrentHost` + save()/reload() split.** Core of the feature.
   Prove: unit-style check that writing host A then switching to host B yields B's stored values;
   round-trip a value through `save()` and confirm it lands under `hostsettings/<uuid>/`, not the
   flat key. High improvement, high safety (additive, reversible), moderate effort.
2. **Versioned migration (marker -> 2).** Seeds existing hosts from current globals.
   Prove: with a pre-populated global store + N hosts in the array, run reload() once and assert
   every `hostsettings/<uuid>/<key>` equals the global value; run reload() again and assert no
   change (idempotency). Highest safety leverage — this is what protects the owner's live config.
3. **Builder-side `setCurrentHost(computer->uuid)` before `new Session`.** Launch correctness.
   Prove: launch host B while host A was last saved; confirm Session's `m_StreamConfig` carries B's
   width/fps/bitrate (perf overlay or logged config).
4. **Orphan cleanup on host delete + empty-uuid guard.** Hygiene + crash-proofing. Low effort.
   Prove: delete a host, assert its `hostsettings` block is gone; select an unpaired host, assert
   no crash.

NEEDS-OWNER-EYES:
5. **UI relocation (Step 2): move codec/decoder/HDR/YUV444/audio/absolute-mouse/unlock-bitrate
   into PcView; strip connection rows from SettingsView.** Larger visual change; Starlight's
   owner-visual-signoff convention applies. Prove: `bash src-plasma/install.sh`-style live run and
   owner sign-off; confirm nothing formerly reachable becomes unreachable.
6. **`showPerformanceOverlay` scope + render location.** Recommended GLOBAL; owner decides whether
   it stays on the host page or moves to the cog.
7. **CLI precedence re-apply (setCurrentHost then re-apply explicit flags).** Small; only matters
   for `moonlight stream` CLI users.

## Kill list (deliberately NOT doing)

- **Not** introducing a separate `PerHostPreferences` QML object (Option B). Rejected for blast
  radius vs the singleton overlay; violates the restraint directive.
- **Not** re-architecting the QSettings layout or the hosts array. Per-host layers cleanly beside
  them.
- **Not** moving connection prefs into `NvComputer::serialize` / the host-array rewrite cycle.
- **Not** deleting or rewriting the legacy global connection keys — they are the downgrade-safety
  net and the new-host template.
- **Not** adding field-level locking to StreamingPreferences — the single-active-session +
  PcView-hidden invariant (plus optional Session snapshot) covers it.
- **Not** making `windowMode`, `uiDisplayMode`, or gamepad/input-mapping options per-host — they
  are local client behavior, expected consistent everywhere.
- **Not** gating on the app version string or a runtime feature flag.

## Top risks

1. **Migration mis-seed** would silently change the owner's Mac mini config. Mitigated by seeding
   from the just-loaded template and the per-key `contains()` guard; this is the #1 thing the
   adversarial judge/verify must prove (run reload() against a realistic store fixture).
2. **A connection key left out of the PER-HOST subset** silently stays global (or vice-versa) —
   one shared list of PER-HOST SER keys must drive migration, `setCurrentHost`, and `save()` so
   the three cannot disagree. Single source of truth for the subset.
3. **UI unreachability** if SettingsView rows are stripped (Step 2) before PcView gains the
   corresponding controls — sequence Step 1 (store) before Step 2 (relocation), and verify the
   full control inventory survives the move.
4. **CLI regression** if `setCurrentHost` overwrites CLI-parsed overrides — handle via the
   re-apply-explicit-flags order; until then the builder-side call does not touch the CLI path.
