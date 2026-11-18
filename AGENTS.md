# Working on Moonwalk

Read `docs/development.md` for the current workflow and `CONTRIBUTING.md` for the
product/engine boundaries. Historical migration plans are evidence, not mandatory
steps for routine patches. Direct user instructions take precedence.

- Start with the user's outcome and the smallest useful change. For a small fix,
  inspect, reproduce, fix, run relevant checks, and finish. Do not invent approval
  gates, plan documents, agents, or release matrices for routine work.
- Use `./dev` and reuse its build cache. Do not create a fresh build directory or
  rebuild the whole application for each QML edit. `./dev test NAME --function
  Case::test_name` is the tightest feedback loop; `./dev check` is the normal quick
  gate. Use `./dev test --all` when shared behavior warrants broader coverage.
- Full AOT, Qt-floor/Flatpak, native-session and real-stream checks belong to
  release preparation or changes affecting those boundaries. Never claim that
  offscreen tests certify hardware or a real desktop session.
- Add behavior regression coverage when it prevents a real recurrence; don't
  mirror implementation details or add tests for trivial reversible copy/style
  changes. Prefer existing fixtures. Named control lookup uses QtTest `findChild`,
  not a full property-graph walk.
- Tests/app previews must use private XDG state. Never attach tests to production
  preferences, credentials, hosts, or the user's session bus. Never stop the user's
  app to test a patch. Bound heavy local work with the provided runner.
- Run checks once after the final relevant edit. Broaden or repeat only for a
  concrete risk, failure, or new change. Report exact evidence and limitations.
- Preserve worktrees, build caches and unrelated changes. No cleanup of protected
  data, public publication, production installation or repository merging is
  implied by running development checks.
