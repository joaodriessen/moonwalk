# Desktop-native release report

Task 11 began release validation from desktop-native architecture code base
`17c2e268d427718ae4a971ac1e3355a97755199c`. The adaptive shell remains
available for explicit opt-in testing, but it is not release-approved or
approved as the default.
Unset `STARLIGHT_ADAPTIVE_SHELL`, or set it to `0`, to retain the legacy
launcher. Set it to exactly `1` only for a deliberate adaptive test.

## Release decision

The legacy launcher remains the default. Automated behavior, AOT compilation,
fallback, isolated compositor startup, and Flatpak gates pass, but several
hard gates require a controlled physical or logged-in test environment that
was not available. Task 11 keeps the legacy route as the default.

The blockers are:

1. the host and settings pages still reuse characterized app-painted roots
   instead of composing the intended page-wide GNOME/Plasma controls;
2. normal-session Orca tree and spoken-output validation is not complete;
3. no physical controller was available for focus, recovery, and latency;
4. logged-in Plasma Wayland/X11 and real GNOME X11 visual checks are missing;
5. no proven private host/credential clone was available for a legacy versus
   adaptive stream comparison through SDL, decode, first frame, audio, input,
   stop/error recovery, and return to the launcher; and
6. GPU, first-frame, decoder, controller-latency, and stream-recovery metrics
   therefore remain unmeasured.

Synthetic and unit evidence does not override those blockers.

## Automated, build, and fallback gates

A fresh Qt 6.11.1 test build ran the complete `make -j1 check` hierarchy:

| Suite | Result |
| --- | --- |
| Smoke | 3/3 |
| Platform profile | 10/10 |
| Adaptive shell selection and Material bootstrap | 11/11 |
| Plasma gate and style probe | 45/45 |
| GNOME appearance and chrome | 17/17 |
| Settings and migration | 10/10 |
| Controller mapping and routing | 19/19 |
| Workflow parity | 23/23 |
| Session contract | 3/3 |
| QML characterization | 66/66 |
| Aggregate | **207 passed, 0 failed, 0 skipped** |

The same 207/207 hierarchy passed in a clean official Ubuntu Base 24.04.4
environment with Ubuntu's Qt 6.4.2 packages, an AOT application build, and a
managed X11/Openbox test display. This locally reproduces the intended CI
compatibility floor; remote GitHub execution remains pending publication.

Focused release slices also passed: accessibility semantics 15/15, Plasma QML
15/15 in each of eight appearance rows, and GNOME QML 11/11 both offscreen
and under managed X11. The Plasma rows covered generic and Plasma routing,
Fusion and Breeze, light and dark, scale 1.5 and 2, and reduced motion.

Fresh plain and `CONFIG+=qtquickcompiler` builds linked successfully.
`ldd -r` reported no unresolved dependency. Both host Qt 6.11.1 and Ubuntu
Qt 6.4.2 AOT builds produced 62 QML/JavaScript cache objects plus the cache
loader. The host AOT binary is 71,791,648 bytes with SHA-256
`bb2218071081994d254452a89364731adf1e467141c4f5de759b4a9e8d9aa5e5`.
Fresh `embedded` and `config_SL` qmake outputs contain no desktop-adapter
macro and no Qt Widgets or DBus linkage.

The production AOT binary held bounded stable event loops in these isolated
startup checks:

- managed GNOME X11, Material Dense, adaptive, without Wayland decoration;
- managed Plasma X11, `org.kde.desktop`, Fusion, adaptive;
- virtual KWin Wayland, `org.kde.desktop`, Fusion, adaptive;
- an incomplete Plasma style root, which failed closed to Material Dense and
  the adaptive generic route; and
- generic X11 with the unchanged legacy default.

No real stream was started by those checks.

## Flatpak gate

The tracked Flatpak manifest was not changed. A private manifest copy replaced
only the Starlight source with the local worktree and excluded `.git`, private
evidence, generated builds, and the packaging directory. A fresh build
directory used private copy-on-write clones of the prior builder state and
user Flatpak installation, with downloads disabled and two low-priority jobs.
The original caches and installation were never build targets.

Flatpak Builder 1.4.9 required its documented
`--disable-rofiles-fuse` compatibility mode because the host FUSE3 helper
rejects the builder's FUSE2 splice options. The full dependency and
application build then completed, generated all 62 QML/JavaScript cache
objects plus the cache loader, composed
AppStream metadata, cleaned the app, and produced finish metadata for
`org.kde.Platform//6.10`. The stripped sandbox binary is 4,223,088 bytes with
SHA-256
`1669a05a951dc73488c3d5cf5a4801979fbf8654007ca73679df1ed79225df93`.

Uninstalled `flatpak build --readonly` probes then passed:

- SDK `ldd -r`, with no missing library or undefined symbol;
- runtime `starlight --version`, selecting Material Dense; and
- network-unshared six-second legacy and adaptive generic software/offscreen
  starts, each holding its expected event loop until bounded termination.

Nothing was installed, exported to a repository, or applied over the
production Flatpak.

## Scoped performance comparison

Three alternating software/offscreen runs per route measured process startup
to the QML route log and five seconds of no-input launcher activity:

| Route | Startup-route mean | Sampled idle CPU | Mean RSS | Peak RSS |
| --- | ---: | ---: | ---: | ---: |
| Legacy | 834.7 ms | 10.947% | 189,766 KiB | 205,620 KiB |
| Adaptive | 936.0 ms | 7.963% | 189,862 KiB | 194,256 KiB |

The adaptive route was about 101 ms slower to its route log and used about
96 KiB more mean RSS in this narrow launcher test. Its sampled CPU and peak
RSS were lower, but the short software-rendered runs are not a general
performance benchmark. These values do not describe compositor GPU use or
streaming.

## CI and release-source policy

The Linux CI workflow now installs the Qt Quick runtime modules required by
Ubuntu 24.04, gives the AOT build and test hierarchy separate private XDG
roots, disables QML disk caching, and executes the full `make -j1 check`
hierarchy under Xvfb plus a readiness-checked Openbox window manager. The YAML
and every shell block parse locally; the remote GitHub job cannot run until a
later explicitly authorized publication step.

Release-source pinning remains separate from UI work. The tracked Starlight
source still follows public `main`; this unpublished local branch has no
public commit that the manifest can fetch. Pinning it now would either be
unusable or would mix publication policy into the UI branch. Before a public
release, review and select an exact published tag or commit in a separate
change. No Flatpak permission reduction or QAdwaita-pin change is included
here.

## Independent review

The same independent reviewer audited the entire 43-commit range from merge
base `b516b56d3a7f977ddb2840d7438b0069ebc989b5` through release-fix commit
`a11cc79dd7a24a4f2820e3a4023e4d40a5e037c2`. The initial review found zero
Critical, two Important, and one Minor issue: the Ubuntu Qt 6.4 contract was
not executable, legacy inherited Material parity had regressed, and the
public report contained unnecessarily specific backup context.

The review-fix commit added the complete Ubuntu module set and managed-X11
hierarchy, guarded newer QML APIs at the Qt 6.4 floor, restored the inherited
legacy Material contract with live semantic theme/accent bindings, and made
the backup statement generic. The same reviewer then returned a final
verdict of zero Critical, zero Important, and zero Minor findings. The branch
is ready for local handoff with the legacy default retained; that verdict
does not waive any blocked manual release gate.

## Isolation, protected state, and rollback

Every accepted GUI, test, D-Bus session, helper, and Flatpak probe used
nonempty absolute private XDG config, cache, data, and runtime roots around
the outer launcher, with QML disk caching disabled. Two rejected Flatpak
Builder discovery probes demonstrated that the Flatpak wrapper ignores
attempted in-sandbox XDG config/cache/data overrides; they are excluded from
evidence. A before/after inventory found no Builder app-data write, and that
wrapper route was abandoned.

Post-run hashes and metadata match the preflight values for:

- native Starlight config and controller database;
- the complete pre-existing `~/.qttest` namespace;
- protected `build-baseline`;
- the prior Flatpak builder cache and build directory;
- production Starlight and Builder Flatpak app data; and
- the user Flatpak installation and installed Starlight commit.

The backup preimage was unavailable, so no restore was attempted. Detailed
backup diagnostics remain only in the private machine-continuity evidence.

No host package, service, desktop setting, permission, production
installation, user preference, or saved host was changed. The branch was not
pushed, merged, installed, or published.

Operational rollback is immediate and data-free:

```sh
unset STARLIGHT_ADAPTIVE_SHELL
```

Setting `STARLIGHT_ADAPTIVE_SHELL=0` is equivalent. Task 11 adds compatibility
guards for the Ubuntu Qt floor and restores the legacy Material style contract
without changing the route default or migrating data. Revert the focused
Task 11 commits newest first to remove those guards and the release handoff.
The prior milestone reports retain the exact earlier executable rollback
order.

The sole release next action is to run the blocked normal-session,
physical-controller, page-composition, and private real-stream gates in a
controlled test environment. Until all of them pass, do not enable the
adaptive shell by default.
