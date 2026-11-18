#!/usr/bin/env python3
"""Generate GNOME shadow nine-patches from a native GTK render.

This does not reimplement CSS box-shadow. It hands the exact declarations that
libadwaita ships to the installed GTK CSS engine, lets GTK's own renderer
rasterize them, and crops the result into stretchable nine-patch assets. The
Qt 6.4 floor Starlight targets ships no graphical-effects modules, so a
nine-patch is the only way to draw these at all.

Covers the three shadows Starlight needs, quoted in GROUPS below from
libadwaita 1.9.2 and byte-identical to the stylesheet compiled into the
installed libadwaita-1.so.0:

    window.csd, window.csd:backdrop   src/stylesheet/widgets/_window.scss
    .card / list.boxed-list           src/stylesheet/widgets/_misc.scss
    popover.background > contents     src/stylesheet/widgets/_popovers.scss

Where a 1px ring is a *shadow layer* (the window's black 5%, the card's 3%) it
is part of the asset. Rings that are genuinely borders or outlines -- the
window's inset white 7% outline, the popover's black 14% border -- are drawn
separately in QML and are NOT baked in here.

States within a group share one canvas, so switching between them moves
nothing. For the window that is required rather than merely tidy: libadwaita
keeps the active and backdrop extents equal on purpose, which is why its
backdrop shadow opens with a fully transparent layer.

Nothing is assumed to be symmetric or centred. Reach is measured per side, so
the offset card and popover shadows come out correctly taller below than above.

Requires a display; run under Xvfb. GTK/libadwaita must match the pinned
versions (4.22.4 / 1.9.2).
"""
import argparse
import os
import sys

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gsk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Gdk, GLib, Gsk, Gtk  # noqa: E402

from PIL import Image  # noqa: E402

WINDOW_RADIUS = 15  # --window-radius: $button_radius + 6 = 15px
POPOVER_RADIUS = 15  # $popover_radius: $menu_radius + 6 = 15px
CARD_RADIUS = 12     # $card_radius: 12px
BOX = 128          # must exceed twice the corner+blur influence
RENDER_PAD = 48    # generous; the real extent is measured, not assumed
AA_TOLERANCE = 1   # cairo antialiasing noise, measured on a symmetric render

# Each group shares one canvas, so the states inside it are interchangeable
# without anything moving.
GROUPS = {
    "window": {
        "radius": WINDOW_RADIUS,
        "states": {
            # window.csd
            "window_shadow": (
                "0 0 14px 5px rgb(0 0 0 / 15%), "
                "0 0 5px 2px rgb(0 0 0 / 10%), "
                "0 0 0 1px rgb(0 0 0 / 5%)"
            ),
            # window.csd:backdrop
            "window_shadow_backdrop": (
                "0 0 14px 5px transparent, "
                "0 0 10px 5px rgb(0 0 0 / 8%), "
                "0 0 0 1px rgb(0 0 0 / 5%)"
            ),
        },
    },
    "card": {
        "radius": CARD_RADIUS,
        "states": {
            # .card / list.boxed-list (_misc.scss). Cards carry no border --
            # the 1px ring is the first shadow layer.
            "card_shadow": (
                "0 0 0 1px rgb(0 0 6 / 3%), "
                "0 1px 3px 1px rgb(0 0 6 / 7%), "
                "0 2px 6px 2px rgb(0 0 6 / 3%)"
            ),
        },
    },
    "popover": {
        "radius": POPOVER_RADIUS,
        "states": {
            # popover.background > contents -- note the vertical offsets, which
            # make this shadow asymmetric top to bottom.
            "popover_shadow": (
                "0 1px 5px 1px rgb(0 0 0 / 9%), "
                "0 2px 14px 3px rgb(0 0 0 / 5%)"
            ),
        },
    },
}


def render_state(display, state, shadow, radius):
    """Render one shadow state with GTK and return it as an RGBA PIL image."""
    provider = Gtk.CssProvider()
    provider.load_from_data(
        f"""
        window {{ background: transparent; }}
        #shadow-pad {{ background: transparent; }}
        #shadow-box {{
            background: transparent;
            min-width: {BOX}px;
            min-height: {BOX}px;
            border-radius: {radius}px;
            box-shadow: {shadow};
        }}
        """.encode()
    )
    Gtk.StyleContext.add_provider_for_display(
        display, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

    # Keep the box at its natural size inside a larger container, so the
    # shadow has room and GTK's paintable bounds grow to the shadow's ink
    # extents rather than clipping them at the box edge.
    box = Gtk.Box()
    box.set_name("shadow-box")
    box.set_halign(Gtk.Align.CENTER)
    box.set_valign(Gtk.Align.CENTER)
    box.set_hexpand(True)
    box.set_vexpand(True)

    pad = Gtk.Box()
    pad.set_name("shadow-pad")
    pad.set_size_request(BOX + 2 * RENDER_PAD, BOX + 2 * RENDER_PAD)
    pad.append(box)

    win = Gtk.Window()
    win.set_decorated(False)
    win.set_child(pad)
    win.set_default_size(BOX + 2 * RENDER_PAD, BOX + 2 * RENDER_PAD)
    win.present()

    ctx = GLib.MainContext.default()
    for _ in range(400):
        while ctx.pending():
            ctx.iteration(False)
        if pad.get_width() > 0:
            break
        ctx.iteration(True)
    if pad.get_width() <= 0:
        raise RuntimeError(f"{state}: widget never allocated")

    paintable = Gtk.WidgetPaintable.new(pad)
    width = paintable.get_intrinsic_width()
    height = paintable.get_intrinsic_height()
    snapshot = Gtk.Snapshot.new()
    paintable.snapshot(snapshot, width, height)
    node = snapshot.to_node()
    if node is None:
        raise RuntimeError(f"{state}: empty render node")

    renderer = Gsk.CairoRenderer.new()
    renderer.realize_for_display(display)
    texture = renderer.render_texture(node, None)
    tmp = f"/tmp/.gnome-shadows-{state}-{os.getpid()}.png"
    texture.save_to_png(tmp)
    renderer.unrealize()
    win.destroy()
    Gtk.StyleContext.remove_provider_for_display(display, provider)

    image = Image.open(tmp).convert("RGBA")
    image.load()
    os.unlink(tmp)
    return image


def locate_box(image):
    """Find the box edges in the render instead of assuming it is centred.

    An outset shadow is clipped out of its own box, so the box interior is the
    transparent region in the middle. Walking out from the centre until alpha
    rises finds each edge exactly, with no reliance on GTK having centred the
    widget or on the paintable bounds being even.
    """
    width, height = image.size
    alpha = image.getchannel("A")
    row, column = height // 2, width // 2
    if alpha.getpixel((column, row)) != 0:
        raise RuntimeError("box interior is not transparent")

    def walk(getter, start, step, limit):
        index = start
        while 0 <= index + step < limit and getter(index + step) == 0:
            index += step
        return index

    return {
        "left": walk(lambda x: alpha.getpixel((x, row)), column, -1, width),
        "right": walk(lambda x: alpha.getpixel((x, row)), column, 1, width),
        "top": walk(lambda y: alpha.getpixel((column, y)), row, -1, height),
        "bottom": walk(lambda y: alpha.getpixel((column, y)), row, 1, height),
    }


def measure_extents(image):
    """Visible shadow reach per side, in px, outside the box edge.

    Measured per side rather than once, because a shadow with a vertical
    offset -- as the popover has -- reaches further below than above.
    """
    width, height = image.size
    box = locate_box(image)
    alpha = image.getchannel("A")
    row, column = height // 2, width // 2

    def scan(getter, limit, edge, step):
        for index in range(limit):
            if getter(index) > 0:
                return abs(edge - index)
        raise RuntimeError("no shadow rendered")

    return {
        "left": scan(lambda x: alpha.getpixel((x, row)), width, box["left"], 1),
        "right": scan(lambda x: alpha.getpixel((width - 1 - x, row)), width,
                      width - 1 - box["right"], 1),
        "top": scan(lambda y: alpha.getpixel((column, y)), height, box["top"], 1),
        "bottom": scan(lambda y: alpha.getpixel((column, height - 1 - y)), height,
                       height - 1 - box["bottom"], 1),
    }


def emit(image, pads):
    """Crop the GTK render onto the shared nine-patch canvas."""
    box = locate_box(image)
    return image.crop(
        (box["left"] - pads["left"], box["top"] - pads["top"],
         box["right"] + 1 + pads["right"], box["bottom"] + 1 + pads["bottom"])
    )


def detect_borders(crop, pads):
    """Smallest nine-patch borders whose middle bands repeat exactly.

    A nine-patch may only stretch where the edge profile is constant along the
    edge. Near a rounded corner it is not, so each border has to swallow the
    corner radius plus the blur that bleeds out of it. Measure that instead of
    assuming it.
    """
    alpha = crop.getchannel("A")
    width, height = crop.size
    borders = {}
    profiles = {}

    def matches(band, reference):
        # Cairo's antialiasing makes an exactly symmetric shape differ from
        # itself by up to one alpha unit, so an exact comparison would push the
        # border out on noise alone.
        return all(abs(a - b) <= AA_TOLERANCE for a, b in zip(band, reference))

    def horizontal_band(x, depth, from_bottom):
        return [alpha.getpixel((x, (height - 1 - y) if from_bottom else y))
                for y in range(depth)]

    def vertical_band(y, depth, from_right):
        return [alpha.getpixel(((width - 1 - x) if from_right else x, y))
                for x in range(depth)]

    for side, from_far, depth in (("top", False, pads["top"] + 2),
                                  ("bottom", True, pads["bottom"] + 2)):
        reference = horizontal_band(width // 2, depth, from_far)
        border = width // 2
        for x in range(width // 2, 0, -1):
            if not matches(horizontal_band(x, depth, from_far), reference):
                break
            border = x
        borders[side] = border
        profiles[side] = reference

    for side, from_far, depth in (("left", False, pads["left"] + 2),
                                  ("right", True, pads["right"] + 2)):
        reference = vertical_band(height // 2, depth, from_far)
        border = height // 2
        for y in range(height // 2, 0, -1):
            if not matches(vertical_band(y, depth, from_far), reference):
                break
            border = y
        borders[side] = border
        profiles[side] = reference

    return borders, profiles


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out-dir",
        default=os.path.join(os.path.dirname(__file__), "..", "..", "app", "res"),
    )
    args = parser.parse_args()
    out_dir = os.path.abspath(args.out_dir)

    Gtk.init()
    display = Gdk.Display.get_default()
    if display is None:
        print("no GdkDisplay; run under Xvfb", file=sys.stderr)
        return 2

    sides = ("left", "right", "top", "bottom")
    for group, spec in GROUPS.items():
        renders = {
            state: render_state(display, state, shadow, spec["radius"])
            for state, shadow in spec["states"].items()
        }
        extents = {s: measure_extents(i) for s, i in renders.items()}
        for state, extent in extents.items():
            print(f"{group}/{state} measured reach: "
                  + ", ".join(f"{k}={extent[k]:g}px" for k in sides))

        # One shared canvas for the whole group, so its states are
        # interchangeable at runtime without anything moving.
        pads = {k: int(max(e[k] for e in extents.values()) + 0.5) for k in sides}

        crops = {}
        borders = {k: 0 for k in sides}
        for state, image in renders.items():
            crops[state] = emit(image, pads)
            state_borders, profiles = detect_borders(crops[state], pads)
            for side in sides:
                borders[side] = max(borders[side], state_borders[side])
            print(f"{group}/{state} edge alpha (outer->inner): "
                  + "; ".join(f"{k}={profiles[k]}" for k in ("top", "left")))

        width = BOX + pads["left"] + pads["right"]
        height = BOX + pads["top"] + pads["bottom"]
        if borders["left"] + borders["right"] >= width \
                or borders["top"] + borders["bottom"] >= height:
            raise RuntimeError(f"{group}: nine-patch borders exceed the canvas")

        for state, crop in crops.items():
            path = os.path.join(out_dir, f"{state}.png")
            crop.save(path, optimize=True)
            print(f"wrote {path}  canvas={width}x{height}")

        print(f"  {group} QML wiring: "
              + ", ".join(f"{k}Margin=-{pads[k]}" for k in sides))
        # Borders are a floor, not an exact figure: anything larger simply
        # leaves less of the (already uniform) edge to stretch.
        print("                  minimum border { "
              + "; ".join(f"{k}: {borders[k]}" for k in sides) + " }")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
