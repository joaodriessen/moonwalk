# Development: a short feedback loop

Use `./dev` from a checkout with the Qt 6 build dependencies installed. Python 3,
qmake6 and make are the only runner dependencies. See the
[build prerequisites](building.md).

```sh
./dev check                         # quick host checks; normal stopping point
./dev test tst_gnomepages            # only the page being changed
./dev test tst_gnomepages --function test_bitrateSliderSurvivesDragAndFollowsPreferences
./dev test workflows                # one C++ suite
./dev check --changed main          # quick checks plus affected suites
./dev test --all                    # all host tests, still no app rebuild
./dev qmlcheck                      # production QML compiler, no C++ app build
./dev build                         # incremental local app, no AOT
./dev build --aot                   # separate release/AOT build
./dev list                         # available suite names
```

Use an actual function name from the chosen test file with `--function`.
`./dev --help` lists the remaining options. Native KDE-style checks can use
`./dev test tst_plasmapages --style org.kde.desktop` when the style is installed.

## See the interface

`tests/preview/preview.py --desktop gnome|plasma|generic --scheme light|dark --out DIR [--scenes a,b]`
renders named scenes of the production `main.qml` against the test doubles, with private
state and no display or session bus. The scene list is in `tests/preview/Driver.qml`. It
builds separately and is not part of the app or `./dev`. KDE scenes need the Breeze and
Kirigami packages. Offscreen renders show layout, not window frames or compositor behavior.
`--contrast`, `--rtl`, `--accent COLOR`, `--font` and `--scale` render the same scenes under those
conditions; a scene that throws is logged and skipped, and any scene/capture
failure makes the command exit unsuccessfully. Use a fresh output directory so
files left from an older run cannot be mistaken for new captures.

## What happens automatically

- Builds persist outside the checkout in `$XDG_CACHE_HOME/starlight-dev/`
  (normally `~/.cache/starlight-dev/`), keyed by checkout path and Qt version.
  `STARLIGHT_DEV_ROOT` or `--build-dir` can select a cache explicitly. The runner
  refuses to mix sources or Qt versions and locks a cache against concurrent writers.
- Only the requested test targets are built. QML-only work reuses the QML runner;
  C++-only checks do not build the QML runner or the streaming application.
  qmake/make still check dependencies on every invocation, so caching is not
  permission to run stale code. If installed, ccache accelerates C/C++ compilation.
- Every test process gets fresh private XDG config/data/cache/runtime directories.
  It uses offscreen software rendering and cannot attach to the user's display or
  session bus. No production hosts, preferences, or credentials are copied.
- Desktop builds/tests run in a transient systemd user service capped at 3 GiB
  memory-high / 4 GiB memory-max. CI/containers can use `--no-scope` when they own
  resource containment. Compilation uses the available CPUs; tests use four
  processes by default (`--jobs` changes test concurrency only).
- A hung test is terminated with its process group after 120 seconds. Logs are
  written while it runs, retained under the printed results directory, and include
  commands. Test runs also write `summary.json` with duration, exit status, pass/fail/skip counts and
  log paths. Missing totals, timeout, or a test failure makes the command fail.

Do not delete caches as a debugging ritual. Select a different `--build-dir` only
when investigating a cache/build discrepancy. No fresh worktree is needed for each
edit. No global settings changes, desktop startup, full Flatpak rebuild, or clean
application rebuild belong in the ordinary edit/test loop.

## Choose checks by the change

| Change | Enough during development | Broaden when |
| --- | --- | --- |
| Documentation or comments | Review the diff and links | Instructions change executable behavior |
| Copy, icon, spacing or color | Inspect the affected rendered surface | Interaction or shared components changed |
| One page's behavior | Its QML suite or relevant function | Shared routes/focus/state are affected |
| Shared QML or UI state | `./dev check --changed main` | Finishing a cross-page change: `./dev test --all` |
| C++ logic | Relevant C++ suite, then `./dev build` | Shared interfaces or engine behavior changed |
| Build tooling | Runner tests and one real cached invocation | Toolchain or packaging changed |
| Packaging/runtime/decoder/input | Appropriate build plus targeted runtime proof | Preparing a release |

`--changed REF` includes tracked changes relative to REF and untracked files.
Known page paths select their adapter, accessibility and route suites. Shared or
unknown code broadens to all affected host suites; documentation selects none.
It is a conservative convenience, not a substitute for understanding the change.

Add a regression test when it would catch a plausible recurring bug. Prefer an
existing test/fixture over a new harness. Do not add tests that repeat constants,
pin incidental layout implementation, or require a full process matrix for a copy
edit. Keep behavior assertions; fix slow setup and querying before deleting useful
coverage. Use QtTest `findChild` for named controls. The graph walker is reserved
for tests that actually need enumeration; never traverse facade snapshots or Qt
value wrappers as UI objects.

## CI and release certification

Repository links and product-version consistency are checked even on documentation-only changes.
Unknown non-documentation inputs, resources and nested submodule updates trigger a full app build.

Pull requests run the host test hierarchy on Ubuntu 24.04's Qt compatibility floor.
They also run the production QML compiler and validate resource paths, without
compiling/linking the C++ application. They build the complete application only
when C++/build/dependency/resource inputs change.
QML/copy edits do not pay for a complete AOT streaming-client build. The workflow's
manual `release` option and weekly run add a full AOT build regardless of paths.
CI caches compiler output, not test results. Local commits never count as remote
CI execution; inspect the actual CI result after publication.

Release certification is explicit, before distributing a build: AOT app build,
Qt-floor and shipped Flatpak-runtime checks, relevant native desktop/style tests,
and real streaming/input checks for changes in those areas. The specialized
`scripts/validation/` display/style/release harnesses remain available for that
purpose. Offscreen tests do not prove compositor, accessibility speech, physical
controller, hardware decode, or streaming behavior.

The adaptive native shell is already the default, with legacy fallback. Historical
Task 11 checklists describe the original migration, not work required for every
subsequent patch. The existing Flatpak packaging/runtime remains the production
delivery path; this workflow does not install anything.

## Local Flatpak candidates

For an exact local Flatpak candidate, stage a clean committed revision instead of
pointing the manifest at a working checkout (Python 3.12+ and PyYAML required):

```sh
python3 scripts/stage-flatpak-candidate.py --revision HEAD \
  --output /path/to/new-candidate --workspace /path/to/existing-flatpak-workspace
python3 -m unittest discover -s tests/tools -p 'test_stage_flatpak_candidate.py'
```

The helper archives tracked source and each recursive submodule's pinned commit,
copies the manifest's local patches, supplies the exact build revision to qmake,
and records hashes in `provenance.json`. Submodules must already be initialized;
the helper never fetches. Dirty source or an existing candidate directory is an
error. Ignored personal files and Git/worktree metadata are excluded; tracked
private agent state and common credential filenames are rejected, including in
submodules. Local manifest inputs cannot overwrite staged source or provenance.
The force-clean build directory must not overlap the checkout, candidate, export
repository or builder state. These guards do not inspect arbitrary secret values.
It prints
build/export commands using the workspace's existing builder state and build
directory (`--build-dir` can select another existing directory); it does not run
them or install the app. Run `build_command` from `build_working_directory`:
the relative `manifest.yaml` name is resolved in that directory. Flatpak Builder
canonicalizes manifest paths, so this alone does not preserve cache reuse across
different candidate directories. Confirm dependency cache hits before an expensive
build. Dependency hashes describe recorded manifest inputs,
including tag references, and do not claim to resolve floating tags.

## Finishing a change

Inspect the diff, run the relevant checks once, and state what passed and what was
not exercised. Retest after another change or an unresolved failure, not merely
because a commit was created. Small fixes do not need a separate design document,
multi-stage review ceremony, screenshot tour, or full release certification.
Write a short design only for changes whose tradeoffs need recording. Keep private
machine evidence outside the repository and link it from the machine's continuity
record where applicable.

The `nvpairing` C++ suite covers pairing protocol bounds and the raw clipboard/cursor
HTTP seam. Those endpoints require identity encoding and finite `Content-Length`
for entity-bearing responses; 204/304 retain their legal bodyless semantics. Qt
can report completion on truncated chunked/connection-close bodies, so chunked,
compressed and unframed entity responses are rejected. Skylight's Simple-Web-Server
response writer supplies the required length. Complete fixed-length 404/503 remain
status-preserving responses. This compatibility restriction belongs to the raw
extension seam, not normal server-info/launch HTTP.

See the [release checklist](releasing.md) before packaging for other users.
