# Repository privacy

Keep personal machine records, raw logs, pairing data, and coding-agent state
outside GitHub, including in private branches and pull requests.

- Use example hosts, accounts, and addresses in docs and tests. For IPv4
  examples, use a documentation address such as `192.0.2.10`. Render
  screenshots from the [synthetic preview](screenshots.md).
- Before sharing a report or artifact, remove real host names, addresses,
  usernames, device identifiers, local paths, certificates, and keys. Include
  the smallest amount of information that explains the problem.
- Use a project or GitHub no-reply email in new commits. Preserve upstream
  attribution and license notices. Bring reviewed source changes across as
  patches so private Git history stays in its original repository.

Run `python3 scripts/check-privacy.py` before pushing. It checks the Git index
for common secret formats, private paths, and certain image metadata. Enable
the local push guard with `git config --local core.hooksPath .githooks` to check
every new commit, its message, and its author and committer addresses. When
enabled, Actions checks new commits and the proposed tree and summarizes
findings without filenames. Read the diff, commit messages, images, and any GitHub
attachments yourself as well.

If something private reaches GitHub, contact a maintainer privately. Removing
it from the current tree can leave copies in Git history or attachments.
Follow [Security](../SECURITY.md) for vulnerabilities.
