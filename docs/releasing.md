# Preparing a release

Decide separately when to publish source, distribute a tester build, and call
a release stable. Keep the app ID and settings identity so an upgrade can
reuse existing settings and pairings.

## Check the repository

```sh
python3 scripts/check-repository.py
python3 scripts/check-privacy.py
python3 -m unittest discover -s tests/tools -v
```

The repository check verifies links, version agreement, and common private
filenames. The privacy check scans the Git index for common secrets, paths, and
image metadata. Review history, submodules, and attachments separately. The
product version comes from `VERSION` and `STARLIGHT_VERSION_STR` in `app/app.pro`. `app/version.txt`
tracks the inherited engine version.

## Before opening the repository

- Read every exposed branch, tag, issue, pull request, and release surface.
  Review screenshots, attachments, Actions logs, and downloadable artifacts.
  Follow [repository privacy](privacy.md) and rotate any exposed credentials.
- Prepare a monitored private [security reporting](../SECURITY.md) route.
  GitHub offers its form on public repositories. If using it, enable it and
  verify it from a non-maintainer account as soon as the repository is public,
  before announcing it. [GitHub setup guide](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository)
- Review corresponding source, package identity, dependency and artwork
  notices, and distribution rights. Check [third-party notices](third-party-notices.md)
  against the candidate and keep the upstream credits.
- Make sure the README, images, compatibility page, and changelog describe
  the revision you intend to expose. Label synthetic previews in the
  [screenshot workflow](screenshots.md).
- Build from a clean recursive clone. After the visibility change, verify
  anonymous access to the repository, submodules, and required source inputs.

## Build and test the candidate

Run **Flatpak candidate** from GitHub Actions to build the selected revision on
a fresh runner. The workflow uploads the bundle, portable source archive,
checksums, and verification report. Keep them together. Before distributing an
older bundle, verify that its exact corresponding sources are available too.

1. Commit the candidate. Run the relevant [development checks](development.md),
   including the shipped runtime, AOT build, and desktop and streaming checks
   for the affected features. Use test data and isolated settings.
2. For a local build, [stage the committed candidate](development.md#local-flatpak-candidates).
   Keep its source revision, dependency inputs, runtime, checksums, and test
   results together.
3. For the Git-based Flatpak recipe, pin a reachable commit with the intended
   app and run `python3 scripts/check-repository.py --check-flatpak-pin`.
   This checks source inputs and the installed root license against that
   revision. Test access to the pinned source separately.
4. Verify the exported file against the staged candidate. With Flatpak,
   OSTree, and the matching platform runtime installed, run:

   ```sh
   python3 scripts/verify-flatpak-bundle.py /path/to/candidate /path/to/Moonwalk-build.flatpak
   ```

   The check verifies the staged files, imports the bundle into a temporary
   repository, and runs its version command in an isolated runtime. It checks
   the app ID, release version, source revision, launcher, declared license
   files, and sandbox settings against the recipe. The report includes the
   checksum, runtime commit, and permissions
   read from the exported bundle. Review those permissions and test a fresh
   install, an upgrade, and a rollback with private profiles. Check that
   settings and pairings survive and stream through the paired host after
   each change. Keep the previous usable bundle. [Install and rollback](guides/getting-started.md#update-or-remove-moonwalk)
5. Publish the tested bundle with its checksum, corresponding source, and
   release notes after the release decision. Leave earlier release assets
   intact so existing downloads remain identifiable.

## Archive the Flatpak sources

The manual Flatpak candidate workflow puts a portable source archive and its
checksum beside the verified bundle. To build the archive locally, use the
same staging directory and export repository as the bundle.

For a release build, add `--bundle-sources` to the staged Flatpak Builder
command. It prepares a source ref in the same export repository. Use that ref
to make the archive, since Flatpak Builder’s raw source ref contains absolute
paths to its Git cache.

```sh
python3 scripts/archive-flatpak-sources.py "$candidate" "$workspace/repo" \
  Moonwalk-sources.tar.gz > Moonwalk-sources.json
```

The archive includes the staged app and submodule sources, build recipe,
patches, pinned downloads, and dependency Git objects. It checks the recipe
and staged files against the build’s source ref and leaves out generated Git
remotes, fetch records, and hooks. Review the archive before sharing it.
Record its checksum alongside the verified Flatpak. The KDE SDK and platform
come from the runtime distributor.

To test a source archive, extract it in a fresh directory and use its `git`
and `downloads` directories as the starting Flatpak Builder state. Run
Flatpak Builder against `manifest/manifest.yaml` with `--disable-updates`
and `--disable-download` in an offline test environment that already has
the SDK. Use a fresh build directory to check that the archive is complete.

## Record what you tested

Record the exact client OS, desktop, GPU, driver, and host build. Note which
checks passed, failed, or were skipped. For affected paths, exercise the
initial display size and scale, frame delivery under motion, physical input,
disconnection and recovery, and eventual host cleanup.

Review dependency updates and rerun the affected checks before merging.
Flatpak module changes need the same review, including dependencies
Dependabot doesn't track. The scripts under `scripts/validation/` can reproduce
specific historic checks if you have their fixtures. Start with `./dev` for
routine work.

Flathub distribution is a separate step. Put the chosen example screenshots at
public HTTPS URLs that serve the image files directly. Add them to the AppStream
metadata and check that the store build mirrors them. Use the
[screenshot guidelines](https://docs.flathub.org/docs/for-app-authors/metainfo-guidelines#screenshots)
and recheck the
[submission requirements](https://docs.flathub.org/docs/for-app-authors/requirements)
for app ID, permissions, licensing, supported architectures, and AI disclosure.
Keep signing and upload credentials out of untrusted pull request jobs.
