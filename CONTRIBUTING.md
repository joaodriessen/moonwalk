# Contributing to Moonwalk

Bug reports, fixes and clear ideas are welcome. You don't need to write code to help: a clear report from a computer we haven't tried is very useful, and [the issue forms](https://github.com/joaodriessen/moonwalk/issues/new/choose) make that easy.

For a large change, open an issue first so we can agree on the problem and approach.

## Work on the code

Moonwalk is a fork of Moonlight Qt. Most interface code is in `app/gui/`; streaming and networking code is in `app/streaming/` and `app/backend/`. Keep changes focused so updates from Moonlight Qt remain manageable. [Architecture](docs/ARCHITECTURE.md) explains the boundaries.

The normal local check is:

```bash
./dev check
```

For a specific change, use `./dev test NAME` and `./dev build` as needed. [Development workflow](docs/development.md) has the full commands. To build from source, see [building](docs/building.md).

## Send a change

Explain what changed, why, and how you checked it. Include a screenshot for visible interface changes. Keep personal settings, hostnames, IP addresses, credentials, and logs with private data out of commits and attachments.

Moonwalk calls the remote computer a **host** in its interface. Some internal names inherited from Moonlight Qt still say `PC`; changing those names alone makes upstream updates harder, so leave them unless the code needs to change.

## Review and evidence

The author is responsible for every submitted change, including AI-assisted work.
Explain the behavior, keep patches reviewable, and report checks actually run and
remaining limits. Never invent test results or include private prompts, session
logs, credentials, or machine diagnostics. Do not add new dependencies without
explaining their purpose and license.

Security reports follow [Security](SECURITY.md), not ordinary bug reports.

## Keep contributions safe to share

Use synthetic hosts, addresses and accounts in examples, captures and tests.
Keep personal machine logs, pairing material, private histories and local tooling
state out of commits and GitHub attachments. Review the [privacy rules](docs/privacy.md)
before opening an issue, submitting a patch or adding a release asset.
