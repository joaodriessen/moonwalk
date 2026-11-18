# Contributing to Moonwalk

If something breaks, tell us what you saw and how to reproduce it. Reports
from different GPUs, desktops, and hosts help us find gaps in the current
testing. Use the [issue forms](https://github.com/joaodriessen/moonwalk/issues/new/choose).
For a larger feature or change in behavior, discuss the problem in an issue
before spending time on a patch.

## Work on the code

Start with [Building Moonwalk](docs/building.md) and the
[development workflow](docs/development.md). Run `./dev test NAME` for the
affected area or `./dev check` for the usual local check.

The interface lives under `app/gui/`. Streaming and host communication are
under `app/streaming/` and `app/backend/`. Keep engine changes focused so
we can compare them with Moonlight Qt. [Architecture](docs/ARCHITECTURE.md)
explains where shared UI and protocol behavior belong.

Call the remote computer a *host* in the interface. Some inherited identifiers
still say `PC`. Keep those identifiers when editing the copy around them.

## Send a patch

Explain what changed, why, and how you checked it. For a visible change,
show the affected screen with an example host. Explain what new dependencies
are for and include their licenses. The same review and testing standards
apply to AI-assisted changes.

Use fictional hosts and documentation addresses in tests and examples. Read
[repository privacy](docs/privacy.md) before sharing diagnostic text, screenshots,
or build artifacts. Follow [Security](SECURITY.md) for vulnerabilities.
