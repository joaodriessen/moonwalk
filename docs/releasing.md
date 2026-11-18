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
- Verify a monitored private [security reporting](../SECURITY.md) route.
- Review corresponding source, package identity, dependency and artwork
  notices, and distribution rights. Check [third-party notices](third-party-notices.md)
  against the candidate and keep the upstream credits.
- Make sure the README, images, compatibility page, and changelog describe
  the revision you intend to expose. Label synthetic previews in the
  [screenshot workflow](screenshots.md).
- Build from a clean recursive clone. After the visibility change, verify
  anonymous access to the repository, submodules, and required source inputs.

## Build and test the candidate

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

   The check imports the bundle into a temporary repository and runs its
   binary's version command in an isolated runtime. It checks the app ID,
   release version, source revision, launcher, and GPL notice, then prints a
   checksum and runtime commit. Review permissions and test a fresh install
   and an upgrade with private profiles separately. Check that settings and
   pairings survive, run the affected streaming paths with that same bundle,
   and keep the previous usable bundle.
5. Publish the tested bundle with its checksum, corresponding source, and
   release notes after the release decision. Leave earlier release assets
   intact so existing downloads remain identifiable.

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

Flathub distribution is a separate step. Recheck the
[submission requirements](https://docs.flathub.org/docs/for-app-authors/requirements)
for app ID, permissions, licensing, supported architectures, and AI disclosure.
Keep signing and upload credentials out of untrusted pull request jobs.
