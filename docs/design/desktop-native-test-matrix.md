# Desktop-native test matrix — historical migration record

> Superseded as the daily development policy by [Development](../development.md).
> The native shell was subsequently merged and made the default, with automatic
> legacy fallback. The Task 11 decisions below record that earlier migration
> stage; they do not require repeating this matrix for ordinary changes. Use
> current release requirements when preparing a distribution build.

This matrix is the release gate for the adaptive desktop work. Automated tests
are necessary but cannot prove compositor, accessibility, decoder, or input
behavior; the manual rows remain required before changing the legacy shell's
default.

## Task 11 decision

The adaptive shell remains available as an explicit opt-in, but the legacy
shell remains the default. The automated, build, fallback, and isolated
compositor gates pass. Release approval and the default switch are blocked by
page-wide native composition, normal-session spoken accessibility,
physical-controller validation, logged-in Plasma and real GNOME X11 visual
checks, and a complete private real-stream comparison.

## Automated baseline

| Gate | Command or target | Required result |
| --- | --- | --- |
| Application build | `qmake6 ../moonlight-qt.pro && make -j"$(nproc)"` | Builds the baseline application. |
| AOT QML build | `qmake6 ../moonlight-qt.pro CONFIG+=qtquickcompiler && make -j"$(nproc)"` | QML compiles with `qtquickcompiler`. |
| Qt tests | `qmake6 ../tests/tests.pro && make -j"$(nproc)" && make -j1 check` | Every Qt Test target passes. |
| CI | GitHub Actions Linux job | Runs the AOT build and the separate test hierarchy. |

## Qt-version policy

GitHub Actions intentionally tests the Ubuntu 24.04 Qt packages as an
independent Linux compatibility floor. It is not a substitute for the shipped
runtime: the Flatpak manifest pins `org.kde.Platform` and `org.kde.Sdk` to
`6.10`, and the Flatpak build remains the release/runtime compatibility gate.
Keep AOT QML enabled in both paths. A CI failure is actionable even when the
Flatpak build passes; a Flatpak failure is release-blocking even when CI passes.
Do not change the Flatpak runtime, permissions, or packaging targets as part of
desktop-native test work.

Task 11 reproduced this floor locally in a clean official Ubuntu Base 24.04.4
environment using Ubuntu's Qt 6.4.2 packages. The AOT build and all 207 tests
passed under a managed X11/Openbox display. The remote workflow remains
pending until an explicitly authorized publication step.

## Required manual matrix

| Environment | Appearance and input | Accessibility | Streaming and recovery |
| --- | --- | --- | --- |
| GNOME Wayland | Light/dark, scale, keyboard and controller navigation; dialogs, menus, and window chrome. | Run Orca/AT-SPI script through host selection, pairing, settings, visible errors, and recovery. | Start and stop a real stream; verify video/audio, mouse, keyboard, controller, minimize/restore, and disconnect recovery. |
| GNOME X11 fallback | Repeat the Wayland interaction checks without Wayland-only assumptions. | Repeat the critical Orca/AT-SPI path. | Repeat stream start/stop, input, and recovery. |
| Plasma Wayland | Test active user style, light/dark, scale, keyboard and controller navigation. | Repeat the critical Orca/AT-SPI path. | Repeat stream start/stop, input, and recovery. |
| Plasma X11 fallback | Repeat Plasma checks under X11. | Repeat the critical Orca/AT-SPI path. | Repeat stream start/stop, input, and recovery. |
| Generic/wlroots fallback | Verify the generic profile loads with no KDE/GNOME style assumption. | Confirm the exposed semantic controls remain usable. | Repeat a basic stream start/stop and failure recovery. |

Record the Qt version, session type, selected profile, display scale, input
device, stream host, and pass/fail evidence for each manual run. A failed
manual row blocks the corresponding adapter from becoming the default; the
legacy shell remains available as rollback.

## Task 11 execution record

All automated and GUI processes listed here used nonempty absolute private
XDG config, cache, data, and runtime roots. QML disk caching was disabled.
Synthetic displays and software rendering were used where a logged-in session
was unavailable.

| Gate | Task 11 result | Release meaning |
| --- | --- | --- |
| Fresh Qt hierarchy | Host Qt 6.11.1 and clean Ubuntu 24.04/Qt 6.4.2: 207 passed, 0 failed, 0 skipped in each environment | Pass |
| Focused settings migration | 10/10 | Pass |
| Focused controller contracts | 19/19 | Pass for mapping/routing only |
| Focused accessibility semantics | 15/15 | Pass for QML semantics only |
| Missing Plasma/GNOME plugin fallback | 45/45 and 17/17; production incomplete-style smoke failed closed to Material | Pass |
| Plasma appearance matrix | Eight private rows, 15/15 each, including Fusion, Breeze, dark, scale 1.5/2, and reduced motion | Synthetic pass |
| GNOME appearance matrix | 11/11 offscreen/software in the fresh QML suite and 11/11 on managed X11 | Synthetic pass |
| GNOME Wayland production startup | Earlier isolated adaptive event loop reached loadable Qt Adwaita decoration | Startup evidence only; logged-in interaction, accessibility, and stream paths remain blocked |
| Plasma X11 production startup | Stable `org.kde.desktop`/Fusion adaptive event loop | Synthetic pass |
| Plasma Wayland production startup | Stable `org.kde.desktop`/Fusion adaptive event loop on isolated virtual KWin | Synthetic pass |
| Generic/legacy startup | Stable Material Dense legacy event loop | Pass |
| Plain and AOT builds | Fresh Qt 6.11.1 builds linked; clean Ubuntu Qt 6.4.2 AOT build linked; 62 cache objects plus loader and clean `ldd -r` in both AOT environments | Pass |
| CI definition | Ubuntu 24.04 Qt Quick modules, AOT build, and full managed-X11 `make -j1 check` hierarchy, each with private XDG roots | Locally reproduced; remote GitHub execution remains pending |
| Flatpak | Fresh local-source build and sandbox smoke | See the release report |

The process-scoped software/offscreen comparison averaged 834.7 ms to the
legacy startup-route log and 936.0 ms to the adaptive route. Mean RSS was
189,766 KiB versus 189,862 KiB; sampled idle CPU was 10.947% versus 7.963%.
These three-run measurements establish a bounded launcher regression
reference only. They do not measure GPU use, decode, first frame, stream
latency, controller latency, or recovery.

## Gates not executed

The following rows are unambiguously blocked rather than inferred from
synthetic evidence:

- normal-session Orca tree and spoken-output validation;
- a physical controller focus/recovery/latency pass;
- logged-in Plasma Wayland and X11 visual inspection;
- a real logged-in GNOME X11 visual pass;
- a private real-host legacy/adaptive stream A/B covering SDL window creation,
  negotiated decode, first frame, audio, keyboard/mouse/controller input,
  disconnect/error recovery, and return to the launcher; and
- page-wide replacement and visual inspection of the remaining app-painted
  host/settings surfaces.

No proven private host/credential clone or physical controller was available.
An already-running user stream was observed but never stopped, redirected, or
reused as test state. These blocked rows require the legacy-default decision.
