# Documentation screenshots

The current documentation images use production QML with synthetic hosts and
preferences from [the preview driver](../tests/preview/Driver.qml). They do not
connect to real hosts, use saved pairings or demonstrate measured performance.
The renderer creates private XDG state and disconnects from the session bus.

[Capture provenance](assets/screenshots.json) records the render input
fingerprint, Qt version, image dimensions and SHA-256 values. These are offscreen
software renders, not proof of native compositor decorations or hardware support.

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

The preview command exits unsuccessfully if a scene throws or a capture cannot
be saved. It can still save other successful scenes, so do not publish a partial
set after a failed run. Inspect both themes, dialogs and text wrapping before
committing. App icons and social-preview artwork are separate assets; their
provenance is not established by this capture process.
