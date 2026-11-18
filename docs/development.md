# Developing Moonwalk

Install the [build dependencies](building.md), then run `./dev` from the checkout.
Use the existing build cache for day-to-day work.

```sh
./dev list                            # list suites
./dev test workflows                  # run one C++ suite
./dev test tst_gnomepages             # run one QML suite
./dev check                           # quick checks across the app
./dev check --changed main            # quick checks and suites affected since main
./dev test --all                      # all host-side suites
./dev qmlcheck                        # compile production QML resources
./dev build                           # build the app incrementally
./dev build --aot                     # build the app with ahead-of-time QML
```

Pass `--function` to run one case. For QML, use `CaseName::function_name`.
`./dev --help` lists all options. With the KDE style installed, use
`./dev test tst_plasmapages --style org.kde.desktop` to exercise that style.

## Preview the interface

```sh
python3 tests/preview/preview.py --desktop gnome --scheme light \
  --scenes hosts-ready --out /path/to/new-preview
```

The preview renders production QML with synthetic hosts. Choose `gnome`,
`plasma`, or `generic` and `light` or `dark`. Plasma requires Breeze and
Kirigami. See [screenshots](screenshots.md) for scene names and how to refresh
the documentation captures. These are offscreen renders. Check a real desktop
for window decorations, device input, and compositor behavior.

## Build and test isolation

Builds are stored under `$XDG_CACHE_HOME/starlight-dev/` (normally
`~/.cache/starlight-dev/`), keyed by checkout and Qt version. Use `--build-dir`
or `STARLIGHT_DEV_ROOT` to choose another directory when investigating a cache
problem. The runner locks each cache and refuses to mix Qt versions or sources.

The runner builds only the requested targets and checks dependencies on each
run. It uses ccache when available. Builds and tests use temporary XDG state.
Tests render offscreen with software and use no production settings, pairing
credentials, desktop session, or session bus.

On a systemd desktop, the runner uses a transient user service with 3 GiB
memory-high and 4 GiB memory-max. Use `--no-scope` in CI or a container that
already limits resources. Compilation uses the CPUs available to the process.
`--jobs` changes the number of concurrent tests, which defaults to four.

Test processes have a 120-second limit. Read logs and `summary.json` in the
printed results directory. The summary records duration, exit status, and
pass, fail, and skip counts.

## Choose what to run

| Change | Start with |
| --- | --- |
| Documentation or comments | Read the diff and run the repository checks. Try any new or changed command. |
| Interface copy or styling | Check the affected page and run the QML compiler. |
| A single workflow | Run the relevant suite or test case. |
| Shared QML, focus, or state | Run `./dev check --changed main`. Use `./dev test --all` for changes across pages. |
| C++ behavior | Run the focused suite and build the app. Broaden for shared interfaces. |
| Tooling | Run its tests and one real invocation with a reused cache. |
| Packaging, GPU, or input | Build the package and test the affected feature in its runtime environment. |

`--changed REF` considers changes relative to REF and untracked files. Known
page paths select the relevant adapter and workflow suites. Shared or unknown
code selects broader coverage.

Add regression tests where they can catch a failure someone might hit again.
Use QtTest's `findChild` for named controls. Reserve the graph walker for tests
that need to enumerate UI objects, and keep facade snapshots and Qt values out
of that traversal.

## GitHub checks

Every run checks documentation links and headings, screenshot hashes, the
product version, and privacy in the proposed commits.
Application changes also run the tests on Ubuntu 24.04's Qt 6.4.2 and compile
the production QML. Changes limited to QML and JavaScript stop there. A change
to C++, packaging, resources, dependencies, or CI itself adds the full app
build. The weekly run and manual `release` option also build the app with AOT.
GitHub caches compiler output. Tests run again on each applicable change.

The Qt floor checks are separate from the shipped Flatpak runtime. Use the
[release checklist](releasing.md) for package, desktop, and real-stream tests.
Offscreen tests cover workflows and layout but can't measure hardware decode,
physical input, screen reader speech, or stream cadence.

## Local Flatpak candidates

With Python 3.12 or newer and PyYAML, stage a committed revision from a recursive
checkout:

```sh
python3 scripts/stage-flatpak-candidate.py --revision HEAD \
  --output /path/to/new-candidate --workspace /path/to/existing-flatpak-workspace
python3 -m unittest discover -s tests/tools -p 'test_stage_flatpak_candidate.py'
```

The helper archives the chosen commit and pinned submodules, copies the local
patches, and writes hashes and the source revision to `provenance.json` without
embedding the local build paths. Git dependencies require exact commits, even
when a tag is listed. Downloads require SHA-256 checksums.
Initialize the submodules first. Use a clean checkout and a new output
directory. The helper rejects unsafe output paths and common private filenames.
It excludes ignored local files and Git metadata. Review the staged contents
before distributing them.

Run the printed `build_command` from `build_working_directory`, then run the
bundle command. The helper prints both commands. It does not build or install
anything. Use `--build-dir` to select an existing build directory in the
workspace. Flatpak Builder canonicalizes manifest paths, so a new staging
directory can affect cache reuse. If you installed Flatpak Builder from
Flathub, use `flatpak run org.flatpak.Builder` in place of `flatpak-builder` and
pass the staged manifest as an absolute path.

## Finishing a change

Report the checks you ran, the results, and any test gaps. Keep personal logs
and machine details outside the repo. For source and image review, see
[repository privacy](privacy.md).
