# Repository privacy

Prepare every commit, issue, pull request, screenshot and release asset as if
anyone could read it. A private repository is a staging environment for
Moonwalk. Keep personal machine records in your own storage.

- Use invented host names, synthetic fixtures and documentation addresses such
  as `192.0.2.10`. Screenshots should come from the
  [synthetic preview renderer](screenshots.md).
- Remove real addresses, device identifiers, usernames, home paths, pairing data
  and other people's information from reports. Share only the fields needed to
  reproduce the problem. Keep raw logs and diagnostic archives locally.
- Use a project or no-reply commit address. Keep existing upstream credit and
  license notices. Forward reviewed changes from other repositories as patches
  so their private history and tags stay outside Moonwalk.
- Run `python3 scripts/check-privacy.py` before pushing. This check catches
  common credential formats, private paths and image metadata. Review content
  and images yourself, including staged changes, commit messages and binary
  artifacts. Passing the check is one part of review.

Maintainers can turn on the local push check with
`git config --local core.hooksPath .githooks`. It scans each new commit you push
from that checkout and checks its author and committer addresses. GitHub Actions
checks the proposed tree again. Review GitHub issue and release attachments
separately, as Git hooks have no access to them.

Follow [Security](../SECURITY.md) for vulnerabilities. If private information
reaches GitHub, contact a maintainer privately without copying it elsewhere.
Removing it from the latest commit alone leaves older copies accessible.
