# Working on Moonwalk

Read [Development](docs/development.md) for the local workflow and
[Contributing](CONTRIBUTING.md) for the client and engine boundaries. The
design and validation records describe earlier work. Follow current user
instructions for the task.

- Keep the patch focused. For routine fixes, reproduce the problem, make the
  change, and run the checks that cover it.
- Use `./dev` and reuse its build cache. For one case, use
  `./dev test NAME --function FUNCTION`. `./dev check` is the normal quick
  check. Use `./dev test --all` for changes shared across several paths.
- Use full AOT, Qt-floor, Flatpak, native-desktop, and real-stream checks
  when a change affects those paths or you are preparing a release.
  Offscreen tests cover only what they exercise.
- Test behavior that could regress. Reuse fixtures and use QtTest's
  `findChild` for named controls. Walk the property graph only when the
  test needs enumeration.
- Use private XDG settings for tests and previews. Keep production
  preferences, credentials, hosts, the session bus, and the running app
  out of those tests. Use the runner to bound heavy local work.
- Run affected checks on the final change and report results and gaps.
  Preserve worktrees, caches, and unrelated changes. Treat publication,
  production installation, and merging as separate operations.
