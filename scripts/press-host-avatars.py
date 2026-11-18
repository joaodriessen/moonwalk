#!/usr/bin/env python3
"""Make the pressed-leather layers for HostAvatar from the OS glyphs.

A host's tile keeps its own colour (HostAvatar draws that gradient); these layers sit
on top of it and are colour-free: the leather grain and each glyph's cut, as a dark
layer and a light layer. Run from the repository root:

    scripts/press-host-avatars.py

Needs numpy, Pillow and rsvg-convert; uses scripts/press-app-icon.py for the press.
"""
import importlib.util, subprocess, tempfile
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / 'app/res/avatar'
SIZE = 256
GLYPHS = {'os_linux': (REPO / 'app/res/os_linux.svg', .56), 'os_macos': (REPO / 'app/res/os_macos.svg', .56),
          'os_windows': (REPO / 'app/res/os_windows.svg', .50), 'monitor': (REPO / 'app/res/lucide/monitor.svg', .60)}

spec = importlib.util.spec_from_file_location('press', REPO / 'scripts/press-app-icon.py')
press = importlib.util.module_from_spec(spec)
spec.loader.exec_module(press)
N = press.N


def glyph(svg, scale):
    """The glyph as a mask centred on the tile. Lucide outlines arrive as currentColor strokes."""
    text = svg.read_text().replace('currentColor', '#fff')
    with tempfile.TemporaryDirectory() as work:
        src, png = Path(work) / 'g.svg', Path(work) / 'g.png'
        src.write_text(text)
        subprocess.run(['rsvg-convert', '-w', str(int(N * scale)), '-h', str(int(N * scale)), str(src), '-o', str(png)], check=True)
        alpha = np.asarray(Image.open(png).convert('RGBA'), float)[..., 3] / 255
    mask = np.zeros((N, N))
    offset = (N - alpha.shape[0]) // 2
    mask[offset:offset + alpha.shape[0], offset:offset + alpha.shape[1]] = alpha
    return mask


def factor(mark):
    """How the press changes a flat mid grey, as a multiplier in sRGB terms."""
    press.TOP = press.BOTTOM = '#808080'
    press.FLOOR = '#3a3a3a'                       # the cut darkens the host's own colour, as indigo did the icon's
    out = press.press(mark).astype(float).mean(axis=2) / 255
    return out / (128 / 255)


def tile_mask():
    big = Image.new('L', (SIZE * 4, SIZE * 4))
    ImageDraw.Draw(big).rounded_rectangle((0, 0, SIZE * 4 - 1, SIZE * 4 - 1), round(SIZE * 4 * .27), fill=255)
    return np.asarray(big.resize((SIZE, SIZE), Image.LANCZOS), float) / 255


def layers(f, name, shape):
    """Black with alpha where the press darkens, white with alpha where it lightens."""
    f = np.asarray(Image.fromarray(f.astype(np.float32), 'F').resize((SIZE, SIZE), Image.LANCZOS))
    dark = np.clip(1 - f, 0, 1) * shape
    light = np.clip((f - 1) * 1.2, 0, 1) * shape
    for kind, colour, alpha in (('dark', 0, dark), ('light', 255, light)):
        rgba = np.zeros((SIZE, SIZE, 4), np.uint8)
        rgba[..., :3] = colour
        rgba[..., 3] = (alpha * 255 + .5).astype(np.uint8)
        Image.fromarray(rgba, 'RGBA').save(OUT / f'{name}-{kind}.png', optimize=True)


def main():
    OUT.mkdir(exist_ok=True)
    shape = tile_mask()
    plain = factor(np.zeros((N, N)))
    layers(plain, 'grain', shape)
    for name, (svg, scale) in GLYPHS.items():
        # only the cut: divide out the grain so the two layers don't double up
        layers(factor(glyph(svg, scale)) / np.clip(plain, .2, None), name, shape)


if __name__ == '__main__':
    main()
