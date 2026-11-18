# Plasma prototype validation

This report records the first opt-in Plasma adapter. It followed the desktop's
selected widget style. Breeze was one of the styles tested. The adaptive
launcher is now the default; see [Architecture](../ARCHITECTURE.md) for the
current activation and fallback rules.

## Live isolated preview

Launch a locally built adaptive preview in the active Plasma Wayland session
without exposing production Starlight settings:

```bash
scripts/validation/run-live-plasma-preview.sh \
  /absolute/path/to/starlight \
  /absolute/path/to/private-validation-output
```

The launcher keeps Starlight config/cache/data in the isolated root and uses a
bubblewrap read-only bind for the host's active `kdeglobals`. It preserves the
session bus and Wayland environment so Qt/KDE palette and widget-style change
notifications reach the already-open process. `--dry-run` prints the resolved
contract without starting Starlight.

## Startup boundary

The Plasma path is selected before constructing the application object or a
QML engine. Every condition below must be true:

| Gate | Required value |
| --- | --- |
| Build | ordinary desktop Linux with Qt Widgets; not Android, macOS, Windows, `embedded`, or `config_SL` |
| Entry point | normal GUI argument shape (`argv[0]` only), never a CLI action |
| Feature switch | `STARLIGHT_ADAPTIVE_SHELL=1` |
| Profile override | absent or exactly `plasma` |
| Active session | only recognized KDE/Plasma session tokens; any mixed or unknown desktop token fails closed |
| Window system | a running desktop X11 or Wayland connection |
| Requested QPA | empty, one `xcb`, or one Wayland windowed variant; fallback lists and `offscreen`, `minimal`, EGLFS, LinuxFB, or KMS/DRM fail closed |
| Style capability | the complete `org.kde.desktop` module and its extension plugins pass preflight |

Only this path constructs `QApplication` and selects
`QQuickStyle::setStyle("org.kde.desktop")`. It leaves
`QApplication::style()` untouched, so Qt inherits the active user widget
style. Startup logs both the Quick Controls style and the widget-style object
name for diagnostics.

Every failed gate takes the existing safe path: `QGuiApplication`, Material
Dense, and the generic adapter. The choice is immutable for the process.
Runtime QPA verification also fails closed if Qt resolves an unexpected
backend after the preflight prediction.

## Style preflight and fallback

The parent preflight searches the application-directory root first, followed
by the configured QML import roots and Qt's system import root, for
`org/kde/desktop/qmldir`. It requires the candidate module to map and provide
every styled Controls family the app uses:

- Button, CheckBox, ComboBox, Switch, Dialog, DialogButtonBox, Frame,
  ItemDelegate, Label, Menu, MenuItem, Page, Pane, Popup, TextField, Slider,
  ScrollBar, StackView, BusyIndicator, ToolButton, and ToolTip;
- both private dependency `qmldir` files; and
- the public desktop, desktop-private, and QQC2 desktop-style extension
  plugins.

The parent checks that each plugin's QML-extension IID, generated class, and
module URI match its adjacent `qmldir`, but deliberately never loads plugin
code. The selected qmldir's `prefer` record may route the module to its own
compiled Qt resource copy; arbitrary qrc roots are not claimed as enumerated.
The parent then launches the same executable in a private, bounded child mode
with fresh `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, and `XDG_DATA_HOME`, disk cache
disabled, the same QPA/import environment, a real `QApplication`, and
`org.kde.desktop` selected. That child creates an `ApplicationWindow`
containing every Qt Quick Controls family used by the app, so effective import
ordering, plugin registration, type mappings, and delegate construction must
all succeed. A timeout, crash, nonzero exit, malformed module, or
load/registration error rejects the prototype.

This process boundary is the fallback transaction. No KDE plugin is loaded
into the parent before the decision, and every partial child load disappears
when the child exits. The parent can therefore construct the existing
`QGuiApplication` plus Material Dense path without a partially loaded KDE
plugin set.

An intentionally incomplete first import root was exercised with the
production binary. Preflight reported the missing `Button.qml`, selected
`Material Dense`, constructed no Plasma widget style, and held a stable
adaptive generic event loop. No environment cleanup or user intervention is
required for fallback.

## QML adapter

`PlatformAdapter.qml` owns both the generic and Plasma adapter objects and
selects one from the preflight-confirmed, read-only
`plasmaDesktopAdapterActive` context property. An absent property also fails
to the generic route.

The Plasma prototype supplies semantic wrappers for:

- button and navigation button;
- dropdown and toggle;
- dialog and menu;
- text field and slider;
- scroll bar and busy indicator.

These wrappers inherit plain Qt Quick Controls and do not replace the selected
style's background, indicator, handle, popup, padding, or implicit geometry.
They add stable accessible metadata and route keyboard, assistive, and
controller activation through the control's ordinary action.

The current Plasma host/settings slices deliberately reuse the characterized
adaptive workflows and page roots. That proves routing and lifecycle parity,
but it does **not** mean every app-painted `SL*` surface has already been
replaced by a native Plasma wrapper. Page-wide visual migration remains a
release blocker.

## Automated matrix

The fresh Task 9 test build produced these totals:

| Suite | Result |
| --- | --- |
| Existing C++ characterization suites | 75 passed |
| Plasma desktop gate and style probe | 45 passed |
| Full Material QML characterization | 53 passed |
| Aggregate | **173 passed, 0 failed** |

The Plasma QML component selection was then run as a focused 15-entry matrix
(13 component/route/real-tooltip functions plus lifecycle entries):

| Case | Result |
| --- | --- |
| Generic route, Material Dense | 15/15 |
| Plasma route, Material Dense route probe | 15/15 |
| `org.kde.desktop`, Fusion, light, scale 1 | 15/15 |
| `org.kde.desktop`, Breeze, light, scale 1 | 15/15 |
| `org.kde.desktop`, Fusion, dark, scale 1 | 15/15 |
| `org.kde.desktop`, Fusion, light, scale 1.5 | 15/15 |
| `org.kde.desktop`, Fusion, light, scale 2 | 15/15 |
| `org.kde.desktop`, Fusion, reduced motion | 15/15 |

The focused tests compare each adapter with a plain control instantiated by
the same selected style. They verify style geometry, native delegates,
accessible roles/names/states/actions, disabled-state fail-closed behavior,
Space-key activation, controller activation, popup behavior, and reduced
motion. Each KDE row copies a private `kdeglobals` fixture that pins
`widgetStyle` to Fusion or Breeze; it does not rely on
`QT_STYLE_OVERRIDE`, which a `QGuiApplication`-backed Quick Test does not
control in KDE's style bridge. The test reads the native button delegate's
reported `styleName` and fails unless it matches the labelled row. The
reduced-motion fixture also sets animation duration factor zero and verifies
the KDE busy delegate does not run its animation.

The full Material suite remains 53/53. Capability checks around the shared busy
delegate and the real shared tooltip preserve Material's semantic colors while
avoiding invalid `color` writes when `org.kde.desktop` supplies a plain
`Item`.

## Build and production smoke evidence

- A fresh `CONFIG+=qtquickcompiler` build generated cache units for every new
  Plasma QML file and linked the desktop application with Qt Widgets.
- Separately generated `embedded` and `config_SL` app makefiles contain no
  Qt Widgets library, `QT_WIDGETS_LIB`, or
  `STARLIGHT_PLASMA_DESKTOP_ADAPTER`.
- Isolated X11 production-binary smokes reached the adaptive event loop with
  `org.kde.desktop`; one inherited `fusion`, and one inherited `breeze`.
- An isolated virtual KWin Wayland session reached the same adaptive event
  loop with `org.kde.desktop` and the inherited Fusion widget style.
- A Plasma-identified `--version` request on `offscreen` exited successfully
  through Material Dense and never constructed a Plasma widget style.
- The intentionally incomplete-style production smoke automatically used
  Material Dense and the generic adapter.

All production-binary smokes after the isolation audit used throwaway
`XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, and `XDG_DATA_HOME` roots.

## Release blockers

The machine used for Task 9 is not a logged-in Plasma desktop. X11 used Xvfb,
and Wayland used a virtual KWin compositor. These are useful application and
compositor startup checks, not substitutes for the release matrix.

The following remain hard gates before enabling this path by default:

1. a real logged-in Plasma Wayland session and Plasma X11 fallback session,
   with visual inspection across representative system themes;
2. live Orca/AT-SPI inspection and spoken-output validation;
3. a physical controller pass, including focus recovery and latency;
4. a real host stream from launcher through SDL window creation, negotiated
   decode path, first rendered frame, keyboard/mouse/controller input, audio,
   stop/error recovery, and return to the launcher;
5. migration of the intended page-wide controls from app-painted `SL*`
   visuals to the semantic native wrappers, followed by parity validation.

Until those gates pass, the accepted result is the opt-in prototype with
automatic generic fallback. The release/default decision is rejected, not
silently inferred from the green synthetic matrix.

## Rollback

Operational rollback is immediate and has no data migration:

```sh
unset STARLIGHT_ADAPTIVE_SHELL
```

Setting `STARLIGHT_ADAPTIVE_SHELL=0` is equivalent. Either choice retains the
legacy route and Material Dense bootstrap. Removing or breaking the KDE style
module also causes automatic generic fallback, but it is not a recommended
rollback mechanism.

Code rollback is newest-first:
`git revert 024b613ce3d955f409e037b236af7fe86c700b69`, then
`git revert d1d7674896e4cc39d449b76364f64492fefcd824`. No package, service,
production installation, or user preference change is part of this task.
Those commands revert executable behavior. This completion-only validation
record may remain as historical evidence; to remove it too, first revert the
commit named `docs: record Plasma adapter verification`.
