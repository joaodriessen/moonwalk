# GNOME adapter feasibility and validation

Task 10 establishes an opt-in, deeply GNOME-integrated Qt prototype. It uses
an app-owned, Adwaita-faithful control skin and optional Wayland window chrome.
It does **not** use GTK or libadwaita widgets and must not be described as
GTK-native.

The legacy launcher remains the default. The GNOME route is reachable only
through the existing `STARLIGHT_ADAPTIVE_SHELL=1` gate, and page-wide
composition remains incomplete.

## Structural and live state

The structural desktop profile and live appearance are separate:

- `PlatformProfileResolver` chooses the process profile once. Only an
  immutable, unmixed GNOME result can select `GnomePlatformAdapter`; combined
  GNOME/Plasma session identity fails closed to generic.
- Plasma keeps defensive precedence if contradictory adapter context
  properties are ever injected, although production emits only the resolved
  route.
- The appearance singleton may update scheme, accent, contrast, font scale,
  and reduced motion at runtime, but those updates cannot change the selected
  route.
- Invalid or unavailable appearance values return to conservative defaults:
  no scheme preference, GNOME blue `#3584e4`, normal contrast, scale `1.0`,
  and full motion.

The desktop build reads only the settings needed by the implemented skin:

| Portal Settings namespace/key | Use and precedence |
| --- | --- |
| `org.freedesktop.appearance/color-scheme` | Translated from portal values to Qt light/dark values; overrides the Qt style-hint fallback once observed |
| `org.freedesktop.appearance/accent-color` | Validated three-channel accent, otherwise the safe default |
| `org.freedesktop.appearance/contrast` | Standard high-contrast source; overrides the GNOME fallback |
| `org.freedesktop.appearance/reduced-motion` | Standard reduced-motion source; overrides the GNOME fallback |
| `org.gnome.desktop.interface/text-scaling-factor` | Bounded font scale from `0.5` through `3.0` |
| `org.gnome.desktop.a11y.interface/high-contrast` | GNOME fallback until the standardized contrast key is observed |
| `org.gnome.desktop.a11y.interface/reduced-motion` | GNOME `reduce` fallback until the standardized reduced-motion key is observed |

Initial values use asynchronous `ReadOne` calls. Later
`org.freedesktop.portal.Settings.SettingChanged` signals update the same
properties and QML tokens without reconstructing the adapter.
Runtime ingestion starts only for a normal GUI launch that requested the
adaptive shell and resolved the GNOME adapter. Legacy, CLI, generic, Plasma,
and non-desktop paths register inert safe defaults and issue no appearance
reads.

## App-owned Qt controls

The prototype provides semantic Qt Quick wrappers for:

- button and navigation button;
- dropdown and toggle;
- dialog and menu;
- text field and slider;
- scroll bar and busy indicator.

The wrappers apply app-owned Adwaita-informed geometry, neutral surfaces,
accent states, focus and contrast borders, large-text sizing, RTL placement,
text elision, and reduced-motion behavior. They expose stable accessible
roles, names, state, and guarded actions. Assistive, keyboard, and controller
activation converge on each control's ordinary action.

The GNOME host and settings slices deliberately reuse the characterized
adaptive page roots. This preserves a single workflow and lifecycle owner, but
it also means the page-wide launcher is not yet composed from these GNOME
wrappers. The current result is a validated semantic/control seam, not a
claim that the whole application is visually native.

## Optional Wayland chrome

QAdwaita is an optional enhancement, never a build, packaging, or runtime
dependency.

Before constructing the GUI application, the selector:

1. preserves an explicit `QT_WAYLAND_DECORATION` choice;
2. considers Adwaita chrome only for an unmixed GNOME-family Wayland session;
3. reads each candidate plugin's exact metadata key and verifies that the
   plugin can load;
4. prefers loadable `qadwaitadecorations`, then loadable `adwaita`; and
5. leaves Qt's functional default untouched when plugins are absent, fail to
   load, the session is mixed or non-GNOME, or the predicted backend is X11.

Tests cover the standalone plugin, Qt's in-tree fallback, absence, standalone
load failure with fallback, total load failure, an explicit user selection,
mixed desktop identity, and GNOME X11. No QAdwaita version is pinned.

## URI integration boundary

The audit found three actual external-URI call sites: the project link, the
release link, and a message-dialog help URL. They continue to use
`Qt.openUrlExternally`, Qt's platform-service abstraction, which can delegate
to the desktop or portal implementation appropriate to the runtime.

Task 10 adds no unused file chooser, notification, document-portal, or
permission surface. No direct shell command or desktop-specific URI launcher
was introduced.

## Automated matrix

A fresh private-XDG test build produced:

| Suite | Result |
| --- | --- |
| Inherited C++ characterization and Plasma suites | 121 passed |
| GNOME appearance and chrome selector | 17 passed |
| Full Material QML characterization | 64 passed |
| Aggregate | **202 passed, 0 failed** |

The focused GNOME QML suite passed 11/11 on each executed backend:

| Backend | Result |
| --- | --- |
| Offscreen/software | 11/11 |
| Logged-in GNOME Wayland | 11/11 |
| Native X11 on Xvfb with a window manager | 11/11 |

The focused tests exercise immutable routing, live light/dark and accent
updates, high contrast, 2x text, reduced motion, RTL placement, narrow text
elision, popup behavior, keyboard Space activation, controller activation,
accessible actions, and disabled/read-only fail-closed behavior.

An isolated accessibility-bus probe used a private session bus, private
runtime directory, Xvfb, and a short-lived visible Qt Quick control. Orca
listed the Qt application through AT-SPI, and the held QML probe passed 3/3.
This proves toolkit registration and discovery only; it does not certify a
normal-session accessibility tree or spoken output.

Mutation checks deliberately broke portal value translation, standardized
contrast precedence, QAdwaita load-failure handling, and the GNOME route.
Each targeted suite failed before exact restoration.

## Build and production smoke evidence

- A fresh `CONFIG+=qtquickcompiler` build generated cache units for all 14 new
  GNOME QML files and linked the desktop application.
- Fresh `embedded` and `config_SL` qmake outputs contain no Qt DBus/Widgets
  libraries or GNOME desktop-adapter macro.
- A private-XDG production-binary smoke in the logged-in GNOME Wayland session
  loaded the available Adwaita decoration, reached the adaptive route, read
  appearance through the portal, and held a stable event loop until bounded
  termination.
- A private-XDG native-X11 smoke on Xvfb with a window manager reached the
  same adaptive route and left Wayland decoration selection untouched.
- Every accepted final matrix, smoke, and helper activation used nonempty
  absolute private `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, and
  `XDG_DATA_HOME` roots. A pre-correction helper probe that omitted the outer
  roots is excluded from evidence; its local continuity audit found no related
  writes, and the corrected rerun passed. The recorded native Starlight
  config/cache metadata and the pre-existing shared Qt-test inventory were
  unchanged before and after the aggregate matrix.
- A corrected production D-Bus monitor observed zero appearance `ReadOne`
  calls from the normal legacy route and exactly the seven scoped reads from
  the requested adaptive GNOME route.
- Repository scans found no copied personal GNOME values, host data,
  credentials, private addresses, or machine-specific identifiers.

## Release blockers

This prototype is implementation-complete for continued testing, not approved
as the default. The following remain hard gates:

1. migrate the intended page-wide surfaces from app-painted `SL*` visuals to
   the GNOME semantic wrappers, then inspect representative light, dark,
   high-contrast, large-text, RTL, and narrow layouts;
2. run the full AT-SPI tree and spoken-output script with Orca in a normal
   logged-in GNOME session;
3. validate a real logged-in GNOME X11 fallback session rather than Xvfb;
4. complete physical-controller focus, recovery, and latency checks; and
5. complete a real host stream through SDL window creation, negotiated
   decode, first frame, input, audio, stop/error recovery, and return to the
   launcher.

Task 11 owns those live gates and the release/default decision.

## Rollback

Immediate route rollback has no data migration:

```sh
unset STARLIGHT_ADAPTIVE_SHELL
```

Setting `STARLIGHT_ADAPTIVE_SHELL=0` is equivalent and retains the legacy
launcher. An explicit `QT_WAYLAND_DECORATION` choice is always preserved.
Complete code rollback is a normal revert of the Task 10 implementation
commit. Task 10 installs no package, service, desktop setting, permission, or
production data.
