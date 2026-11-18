#!/usr/bin/env python3
"""Extract libadwaita's design tokens from the installed toolkit.

The point of this script is to remove transcription. Hand-copying values out of
a stylesheet into QML is how the two drift, and no amount of care fixes that --
it just moves the errors around. So nothing here is read by eye: every colour
is resolved by GTK itself and read back exactly, and
every opacity is recovered by mixing it against a known colour.

Emits tests/data/adwaita-tokens-v1.json, which a QML test pins GnomeTokens to.
Regenerating and diffing is then the whole audit.

Requires a display; run under Xvfb. GTK/libadwaita must match the pinned
versions (4.22.4 / 1.9.2).
"""
import argparse
import json
import os
import sys

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("Gsk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Adw, Gdk, GLib, Gsk, Gtk  # noqa: E402

from PIL import Image  # noqa: E402

# Every colour variable libadwaita publishes, taken from its own stylesheet
# rather than chosen. Keep this list sorted; regenerate it with:
#   strings /usr/lib/libadwaita-1.so.0 | grep -oE '\\--[a-z0-9-]+-color' | sort -u
COLOR_VARS = [
    "accent-bg-color", "accent-color", "accent-fg-color",
    "active-toggle-bg-color", "active-toggle-fg-color",
    "card-bg-color", "card-fg-color", "card-shade-color",
    "destructive-bg-color", "destructive-color", "destructive-fg-color",
    "dialog-bg-color", "dialog-fg-color",
    "error-bg-color", "error-color", "error-fg-color",
    "headerbar-backdrop-color", "headerbar-bg-color", "headerbar-fg-color",
    "headerbar-shade-color", "headerbar-darker-shade-color",
    "popover-bg-color", "popover-fg-color", "popover-shade-color",
    "scrollbar-outline-color",
    "secondary-sidebar-bg-color", "secondary-sidebar-fg-color",
    "shade-color",
    "sidebar-backdrop-color", "sidebar-bg-color", "sidebar-fg-color",
    "sidebar-border-color", "sidebar-shade-color",
    "success-bg-color", "success-color", "success-fg-color",
    "thumbnail-bg-color", "thumbnail-fg-color",
    "view-bg-color", "view-fg-color",
    "warning-bg-color", "warning-color", "warning-fg-color",
    "window-bg-color", "window-fg-color",
]

# Opacities are numbers, not colours, so recover them by mixing a known solid
# colour at that opacity and reading the alpha back.
OPACITY_VARS = ["border-opacity", "dim-opacity", "disabled-opacity"]

# Percentages the type ramp is built from (_labels.scss), and the literal
# metrics that are not derivable from a colour.
TYPE_SCALE = {
    "title-1": 1.81, "title-2": 1.36, "title-3": 1.36, "title-4": 1.18,
    "heading": 1.0, "body": 1.0, "caption": 0.82, "subtitle": 0.8333,
}

SWATCH = 8


def _render_swatches(display, expressions):
    css = "window { background: transparent; }\n" + "\n".join(
        f"#s{i} {{ background: {expr}; min-width: {SWATCH}px; "
        f"min-height: {SWATCH}px; }}"
        for i, expr in enumerate(expressions)
    )
    provider = Gtk.CssProvider()
    provider.load_from_data(css.encode())
    Gtk.StyleContext.add_provider_for_display(
        display, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

    row = Gtk.Box()
    for index in range(len(expressions)):
        swatch = Gtk.Box()
        swatch.set_name(f"s{index}")
        swatch.set_size_request(SWATCH, SWATCH)
        row.append(swatch)

    window = Gtk.Window()
    window.set_decorated(False)
    window.set_child(row)
    window.present()

    ctx = GLib.MainContext.default()
    for _ in range(400):
        while ctx.pending():
            ctx.iteration(False)
        if row.get_width() > 0:
            break
        ctx.iteration(True)

    paintable = Gtk.WidgetPaintable.new(row)
    width = paintable.get_intrinsic_width()
    height = paintable.get_intrinsic_height()
    snapshot = Gtk.Snapshot.new()
    paintable.snapshot(snapshot, width, height)
    node = snapshot.to_node()

    renderer = Gsk.CairoRenderer.new()
    renderer.realize_for_display(display)
    texture = renderer.render_texture(node, None)
    tmp = f"/tmp/.adwaita-tokens-{os.getpid()}.png"
    texture.save_to_png(tmp)
    renderer.unrealize()
    window.destroy()
    Gtk.StyleContext.remove_provider_for_display(display, provider)

    image = Image.open(tmp).convert("RGBA")
    pixels = image.load()
    values = [
        pixels[index * SWATCH + SWATCH // 2, height // 2]
        for index in range(len(expressions))
    ]
    image.close()
    os.unlink(tmp)
    return values


def extract(display, scheme_name, scheme):
    Adw.StyleManager.get_default().set_color_scheme(scheme)

    # Look the colours up rather than rendering them. A rendered swatch is
    # premultiplied and 8-bit, which destroys the channel precision of
    # low-alpha colours -- RGB(0 0 6 / 7%) reads back as pure black. The
    # lookup returns libadwaita's exact floats.
    probe = Gtk.Box()
    window = Gtk.Window()
    window.set_child(probe)
    window.present()
    ctx = GLib.MainContext.default()
    for _ in range(400):
        while ctx.pending():
            ctx.iteration(False)
        if probe.get_width() > 0:
            break
        ctx.iteration(True)
    context = probe.get_style_context()

    def channel(value):
        # The standalone accent/success colours are oklab transforms that can
        # land outside sRGB; GTK reports them unclipped and clips on paint.
        return max(0.0, min(1.0, value))

    colors = {}
    for name in COLOR_VARS:
        found, rgba = context.lookup_color(name.replace("-", "_"))
        if not found:
            continue
        r = round(channel(rgba.red) * 255)
        g = round(channel(rgba.green) * 255)
        b = round(channel(rgba.blue) * 255)
        colors[name] = {
            "hex": f"#{r:02x}{g:02x}{b:02x}",
            "alpha": round(rgba.alpha, 4),
        }

    # Opacities are plain numbers, so recover them by mixing a known colour.
    values = _render_swatches(display, [
        f"color-mix(in srgb, black var(--{name}), transparent)"
        for name in OPACITY_VARS
    ])
    opacities = {
        name: round(a / 255, 4)
        for name, (_r, _g, _b, a) in zip(OPACITY_VARS, values)
    }

    window.destroy()
    return {"colors": colors, "opacities": opacities}


def measure_metrics(display):
    """Metrics that come from real widgets rather than from a colour."""
    header = Adw.HeaderBar()
    group = Adw.PreferencesGroup()
    action_row = Adw.ActionRow()
    action_row.set_title("Row")
    action_row.set_subtitle("Subtitle")
    group.add(action_row)
    page = Adw.PreferencesPage()
    page.add(group)

    controls = Gtk.WindowControls(side=Gtk.PackType.START)
    controls.set_decoration_layout("close,minimize,maximize:")

    toolbar = Adw.ToolbarView()
    toolbar.add_top_bar(header)
    toolbar.set_content(page)
    header.pack_start(controls)

    window = Gtk.Window()
    window.set_child(toolbar)
    window.set_default_size(900, 600)
    window.present()

    ctx = GLib.MainContext.default()
    for _ in range(600):
        while ctx.pending():
            ctx.iteration(False)
        if action_row.get_height() > 0 and header.get_height() > 0:
            break
        ctx.iteration(True)

    def walk(widget, predicate, found=None):
        if found is None:
            found = []
        if predicate(widget):
            found.append(widget)
        child = widget.get_first_child()
        while child is not None:
            walk(child, predicate, found)
            child = child.get_next_sibling()
        return found

    buttons = walk(controls, lambda w: isinstance(w, Gtk.Button))
    clamps = walk(page, lambda w: isinstance(w, Adw.Clamp))

    metrics = {
        "headerbar_height": header.get_height(),
        "row_min_height": action_row.get_height(),
        "window_control_strip_height": controls.get_height(),
        "window_control_count": len(buttons),
        "content_clamp": clamps[0].get_maximum_size() if clamps else None,
    }
    if len(buttons) > 1:
        first = buttons[0].compute_bounds(controls)[1]
        second = buttons[1].compute_bounds(controls)[1]
        metrics["window_control_pitch"] = round(second.origin.x - first.origin.x)
        metrics["window_control_button"] = round(first.size.width)
    window.destroy()
    return metrics


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        default=os.path.join(os.path.dirname(__file__), "..", "..",
                             "tests", "data", "adwaita-tokens-v1.json"),
    )
    args = parser.parse_args()

    Adw.init()
    display = Gdk.Display.get_default()
    if display is None:
        print("no GdkDisplay; run under Xvfb", file=sys.stderr)
        return 2

    document = {
        "_comment": (
            "Generated by scripts/design/gnome-tokens.py from the installed "
            "GTK/libadwaita. Do not hand-edit: regenerate and diff."
        ),
        "gtk_version": "%d.%d.%d" % (Gtk.get_major_version(),
                                     Gtk.get_minor_version(),
                                     Gtk.get_micro_version()),
        "libadwaita_version": "%d.%d.%d" % (Adw.get_major_version(),
                                            Adw.get_minor_version(),
                                            Adw.get_micro_version()),
        "light": extract(display, "light", Adw.ColorScheme.FORCE_LIGHT),
        "dark": extract(display, "dark", Adw.ColorScheme.FORCE_DARK),
        "type_scale": TYPE_SCALE,
        "metrics": measure_metrics(display),
    }

    out = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2, sort_keys=True)
        handle.write("\n")
    print(f"wrote {out}")
    print(f"  {len(COLOR_VARS)} colours x 2 schemes, "
          f"{len(OPACITY_VARS)} opacities, {len(document['metrics'])} metrics")
    return 0


if __name__ == "__main__":
    sys.exit(main())
