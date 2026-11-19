# Contributing to Moonwalk

When something breaks, tell us which client and host builds you used, what
you tried, and what happened. If the issue is visual or affects performance,
include your Linux desktop and GPU. For a larger change, open an issue
describing what you'd like to do before starting a patch. [Issue forms](https://github.com/joaodriessen/moonwalk/issues/new/choose)

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

Tell us what changed, why, and which checks you ran. For an interface change,
show the screen with an example host. If you add a dependency, explain what
it does and include its license. Review and test AI-assisted changes with the
same care as any patch.

Use fictional hosts and documentation addresses in tests and examples. Read
[repository privacy](docs/privacy.md) before sharing diagnostic text, screenshots,
or build artifacts. Follow [Security](SECURITY.md) for vulnerabilities.
