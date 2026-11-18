#!/usr/bin/env python3
"""Make Lucide's glyphs in app/res/lucide loadable as theme icons named lucide-NAME.

KDE's icon loader recolours an SVG to the palette and the control's state when
it carries the "current-color-scheme" stylesheet that Breeze icons use, so each
shape gets that stylesheet's class and strokes with currentColor. The copies go
to app/res/lucide-theme and are compiled in as :/icons/hicolor/scalable/actions,
which Qt and KDE search before the system's icon themes.

    scripts/generate-lucide-theme.py
"""
import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCE = REPO / 'app/res/lucide'
TARGET = REPO / 'app/res/lucide-theme'
STYLE = ('<style id="current-color-scheme" type="text/css">'
         '.ColorScheme-Text { color:#232629; }</style>')
SHAPES = r'<(path|circle|rect|line|polyline|polygon|ellipse)\b'


def themed(svg):
    svg = re.sub(r'^<!--.*?-->\s*', '', svg, flags=re.S)
    svg = re.sub(r'(<svg\b[^>]*>)', r'\1\n  ' + STYLE, svg, count=1)
    return re.sub(SHAPES, r'<\1 class="ColorScheme-Text" stroke="currentColor"', svg)


def main():
    TARGET.mkdir(exist_ok=True)
    for old in TARGET.glob('*.svg'):
        old.unlink()
    for icon in sorted(SOURCE.glob('*.svg')):
        (TARGET / f'lucide-{icon.name}').write_text(themed(icon.read_text()))


if __name__ == '__main__':
    main()
