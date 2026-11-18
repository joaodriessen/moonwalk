# Preparing a release

Source publication, a downloadable alpha and a stable release are different
decisions. This checklist does not authorize publishing, merging or changing
repository visibility. Keep the current app ID and settings/identity storage.

## Repository checks

Run these checks from a Git checkout:

```sh
python3 scripts/check-repository.py
python3 scripts/check-privacy.py
python3 -m unittest discover -s tests/tools -v
```

The first command checks current user/developer documentation links and agreement
between the product version, AppStream and changelog. `app/version.txt` is the
upstream engine version, not the Moonwalk product version. These checks use no
network, credentials, production preferences or running app. It also rejects
tracked agent state, SSH/GnuPG directories and common local credential filenames
at any depth in this repository. This filename guard does not scan file contents,
Git history or submodule contents, and does not replace the publication review below. The second command scans
the staged tree for common privacy risks. See [repository privacy](privacy.md).

The historical `scripts/validation/run-pagewide-stage-gates.sh` requires prepared
private COW/Ubuntu fixtures. It is not a fresh contributor's build command; use
the [development workflow](development.md). Its isolated builder home is synthetic.
`STARLIGHT_FLATPAK_SEED` can explicitly select an existing private COW seed.

## Before making source public

- Review all exposed branches, tags, Git history, issues, comments, attachments,
  screenshots, release assets and Actions logs/artifacts. A clean current tree
  is not proof of clean history. Keep personal machine reports and coding-agent
  state private. Redact or remove sensitive material deliberately; rotate any
  actual exposed credentials. Document test-only key fixtures rather than
  broadly suppressing secret detection.
- Establish a monitored private security route as required by [Security](../SECURITY.md).
  Verify GitHub private vulnerability reporting when enabling the public repository.
- Review GPL source/build availability, dependency and artwork notices, package
  identity and naming. Retain upstream attribution and license texts.
  Check the [notice inventory](third-party-notices.md) against the installed candidate,
  including separately built libraries and its runtime.
- Choose the public branch deliberately. Make README, screenshots, compatibility,
  changelog and download descriptions describe that candidate. Do not present old
  successful CI as evidence for newer commits.
  Follow the [screenshot workflow](screenshots.md) and label synthetic previews accurately.
- Verify a clean recursive checkout builds without private hosts, local caches or
  maintainer signing credentials. After visibility changes, verify anonymous access
  to the repository, submodules and every required source input.

## Build one identifiable candidate

1. Freeze and commit the candidate. Run the relevant tests, shipped-SDK QML/AOT
   compilation and native checks described in [development](development.md).
   Use private test state and fixtures. Never stream to discovered real hosts
   during scripted UI checks.
2. For a local candidate, use `scripts/stage-flatpak-candidate.py` as documented
   there. It snapshots the selected commit and pinned recursive submodules.
   Record the source revision, dependency inputs, runtime, checksums and tests.
3. For the canonical Git-based Flatpak recipe, point its app source to a reviewed
   reachable commit containing the intended application. Run:
   ```sh
   python3 scripts/check-repository.py --check-flatpak-pin
   ```
   This rejects ambiguous/non-Git app sources and untracked app/build inputs, and
   checks app/build inputs and the installed root license against the pinned revision, not anonymous remote
   availability or dependency reproducibility. An old pin is allowed during local
   development but must not silently produce an older public app. Never fabricate
   a new pin before its source is available to builders.
4. Build/export and independently inspect the actual bundle. Verify version,
   payload, permissions and provenance; test fresh install and upgrade with private
   profiles, then the affected streaming paths. Preserve pairings and settings.
   The tested binary must be the published binary. Keep the previous usable bundle.
5. Publish the artifact, checksums, corresponding source and candid release notes
   together only after the release decision. A tag/source version alone does not
   mean a tested downloadable package exists. Do not replace old assets in place.

## Evidence and maintenance

Record exact OS/desktop/GPU/host versions and passed, failed and skipped scenarios.
Include actual motion/pacing, initial scale, input release, interrupted connections
and eventual host cleanup where affected. Unit tests do not certify physical
input, audio, screen-reader speech, display restoration or useful frame cadence.

Keep stable, experimental and unverified capabilities distinct. Only the current
alpha line is maintained; no separate old-version security backports are promised.
Treat a new failure on a previously working supported combination as a regression.
Request a sanitized reproduction before expanding scope or suggesting driver changes.

Track upstream security fixes and local protocol changes separately from UI work.
Dependency update PRs require review and affected checks; do not auto-merge them.
Changes to packaged dependencies in the Flatpak manifest require manual release
review even when Dependabot has no ecosystem for that manifest.

Flatpak remote/Flathub distribution is future work, not an existing updater or
approval. Recheck [Flathub requirements](https://docs.flathub.org/docs/for-app-authors/requirements)
including architecture, permissions, licensing and AI-material disclosure before
submission. Signing/upload credentials must never be available to untrusted PRs.
