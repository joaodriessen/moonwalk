# Documentation screenshots

The [preview renderer](../tests/preview/preview.py) loads production QML
with the scenes in [Driver.qml](../tests/preview/Driver.qml). It uses
temporary settings, example hosts, and no desktop session bus. These captures
show the interface. Check a live session for compositor decorations, hardware,
and actual streaming behavior.

[Capture provenance](assets/screenshots.json) records the source revision,
Qt version, dimensions, and checksums. The render input fingerprint is the
SHA-256 of `git ls-tree -r <revision> -- app tests/preview tests/qml`.

## Refreshing the images

Use the prerequisites in [Development](development.md). Render into a fresh
directory outside the checkout, inspect the results, then replace documentation
assets. For example:

```sh
python3 tests/preview/preview.py --desktop gnome --scheme light \
  --width 860 --height 720 --scale 2 \
  --scenes first-run-empty,hosts-ready,hosts-unpaired,pairing,settings \
  --out /path/to/new-preview/gnome-light
python3 tests/preview/preview.py --desktop gnome --scheme light \
  --width 860 --height 980 --scale 2 --scenes shortcuts \
  --out /path/to/new-preview/gnome-light
```

Repeat with `--scheme dark` and a separate output directory. For Plasma, use
`--desktop plasma --scenes hosts-ready` at 860×720, in both schemes. A scale of 2
keeps text sharp on dense displays; the dimensions above are logical pixels.

| Documentation basename | Preview scene | Desktop |
| --- | --- | --- |
| `first-launch` | `first-run-empty` | GNOME |
| `hosts` | `hosts-ready` | GNOME |
| `pair` | `hosts-unpaired` | GNOME |
| `pin` | `pairing` | GNOME |
| `preferences` | `settings` | GNOME |
| `shortcuts` | `shortcuts` | GNOME |
| `kde` | `hosts-ready` | Plasma |

Assets are named `<basename>-<light|dark>.png`. Preserve the theme pairs and refresh
the provenance when changing them. Keep real addresses, pairing codes, account
names and desktop content out of captures. The visible PIN is a test fixture.

A failed scene or capture makes the command exit nonzero. Check the exit
status before copying any screenshots, since other scenes may have saved. Inspect both themes, dialogs and text wrapping before
committing. App icons and social-preview artwork are separate assets; their
provenance is not established by this capture process.
