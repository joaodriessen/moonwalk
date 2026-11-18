# Generic Controls style baseline

This is the deterministic no-visual-drift gate for the Task 7 single-style
bootstrap. It characterizes the real QML roots rather than equivalent test
fixtures:

- `app/gui/main.qml`
- `app/gui/StreamSegue.qml`
- `app/gui/QuitSegue.qml`
- `app/gui/NavigableMessageDialog.qml`
- `app/gui/starlight/SLBusy.qml`

The test is `tests/qml/tst_stylebootstrap.qml`. It creates all five roots,
opens the real shared tooltip and message dialog, and recursively locates the
busy indicators used by the three state surfaces. Spinner screenshot hashes
are deliberately not used: the indicator is animated and an arbitrary frame
would make a pixel hash timing-sensitive. The stable capture is the selected
style's instantiated delegate geometry, padding, semantic colors, and the
popup scaffold's actual runtime geometry and surface colors.

## Material no-drift baseline

Material remains the single selected generic style, with its established
Dense variant. The deterministic offscreen command is:

```sh
QT_QPA_PLATFORM=offscreen \
QT_QUICK_CONTROLS_STYLE=Material \
QT_QUICK_CONTROLS_MATERIAL_VARIANT=Dense \
./tst_qmlcharacterization \
  -input ../../tests/qml/tst_stylebootstrap.qml -o -,txt
```

From `build-task7-final-tests/qml`, Qt 6.11.1 reports:

| Surface | Captured runtime contract |
| --- | --- |
| Raw selected-style `BusyIndicator` | content delegate 44 × 44 |
| `SLBusy`, Stream, Quit, message dialog | the same 44 × 44 delegate and `Theme.accent` |
| Shared tooltip | vertical padding 8, horizontal padding 16, radius 6, `Theme.darkChip`, white text |
| Message dialog | padding 22, top 22, bottom 4, radius 12, `Theme.cardSolid`; footer padding 14, spacing 8 |
| Main palette | `Theme.ground`, `Theme.label`, `Theme.accent`, and `Theme.accentInk` |

All five runner entries pass (three actual test functions plus lifecycle
entries). The retained Material geometry is compared at runtime to a plain
`BusyIndicator` created by the selected style, so a second explicit style
import cannot silently reappear.

The same Material command is also run with
`STARLIGHT_TEST_FORCE_THEME=dark`. The setup applies the production Theme's
dark-mode inputs before any root is created. All five runner entries pass
with the same 44 × 44/8/16/6/22/22/4/12 geometry; the captured palette is
`#202024` window, `#e6ffffff` label, `#0a84ff` accent, and `#ffffff`
highlighted text. The red run before making the fixture theme-responsive
failed on the real busy and dialog surfaces because they remained on the
light accent and card colors.

## Alternate-style legality probe

Fusion is not a declared product profile. It is a useful adversarial probe
because an explicit Material root would ignore Fusion and expose the original
failure. The corresponding command is:

```sh
QT_QPA_PLATFORM=offscreen \
QT_QUICK_CONTROLS_STYLE=Fusion \
./tst_qmlcharacterization \
  -input ../../tests/qml/tst_stylebootstrap.qml -o -,txt
```

The selected Fusion delegate is 28 × 28; all four application busy states
match it and retain `Theme.accent`. The tooltip keeps radius 6 and its semantic
surface colors after Fusion lazily creates its delegates. The dialog scaffold
remains 22/22/4 with radius 12. All five runner entries pass.

The pre-implementation red run was specific and load-clean: all five roots
instantiated, while `SLBusy` retained Material's 48 × 48 delegate under
Fusion instead of Fusion's 28 × 28 delegate, and `main.qml` had no stable
runtime seam for the real shared tooltip. There were no fixture or import
failures.

## Declared profile behavior

Task 7 intentionally does not infer a Controls style from a desktop profile:

| Profile | Controls style in this phase |
| --- | --- |
| generic | Material Dense |
| GNOME | Material Dense |
| Plasma | Material Dense safe generic path |

`QQuickStyle::setStyle("Material")` and the Dense default are established
before `QGuiApplication` and before any QML engine exists. Plasma does not
select `org.kde.desktop`; that remains gated on the later
`QApplication`/QtWidgets feasibility work.

The production bootstrap checks this contract at that same pre-application
boundary and logs the selected style and variant. With the variant unset,
`app/starlight --version` reports `Qt Quick Controls bootstrap: Material
Dense`. A deliberate rebuild with the production style changed to Fusion
terminated before QML with `Qt Quick Controls style bootstrap did not select
Material`; restoring Material returned the same binary probe to exit 0. An
explicit caller-supplied Material variant remains supported, while the
application-owned default is asserted to be Dense.

QML AOT compilation remains enabled with `CONFIG+=qtquickcompiler`.
`flatpak/com.starlight.Starlight.yaml` continues to set
`QML_DISABLE_DISK_CACHE=1`.

The fresh `build-task7-review-aot/` build generated cache units for all five
roots and linked `app/starlight`. The resulting binary was then started
offscreen with `QML_DISABLE_DISK_CACHE=1` and each explicit
`STARLIGHT_PLATFORM_PROFILE=generic|gnome|plasma` value, using isolated
temporary XDG directories. Each profile loaded the AOT UI and reached a
stable event loop; the temporary credentials, cache, and downloaded probe
data were deleted afterward. A single fresh build is the relevant compile
gate because the immutable runtime profile is input data, not a compiler
configuration, and all three profiles deliberately select the same Material
safe-generic Controls style in this phase.

## Decoration policy inventory

The measured Wayland decoration policy in `main.cpp` is independent of the
desktop-profile resolver and is unchanged:

1. Respect an explicit `QT_WAYLAND_DECORATION`.
2. Only for GNOME/Unity without KDE, scan the Qt plugin path and
   `QT_PLUGIN_PATH` for matching Wayland decoration plugins.
3. Prefer a plugin whose metadata explicitly advertises
   `qadwaitadecorations`; otherwise use `adwaita` when detected; otherwise
   leave Qt's functional default untouched.

KDE/KWin and wlroots-family sessions, including Sway and Hyprland, are not
forced onto Adwaita decorations. Co-installed desktop libraries do not alter
this decision.

## Release boundary and rollback

This deterministic offscreen gate proves root loading, style consistency,
semantic color routing, and stable popup geometry. It does not replace a
normal-session visual check on GNOME, Plasma, and a wlroots compositor, nor
the existing live accessibility and streaming-hardware gates.

Code rollback is one revert of the Task 7 commit. There is no settings or data
migration and no operational rollback. If the later Plasma feasibility gate
fails, Plasma stays on the Material safe-generic path described above.
