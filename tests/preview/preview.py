#!/usr/bin/env python3
"""Render the production UI with test doubles in a private, offscreen environment.

    tests/preview/preview.py --desktop gnome --scheme dark --out DIR [--scenes a,b]

Developer tool only. Private HOME/XDG roots, no session bus or display, and it
can never read production hosts, preferences or credentials.
"""
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
SOURCE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--desktop", default="gnome", choices=["gnome", "plasma", "generic"])
parser.add_argument("--scheme", default="light", choices=["light", "dark"])
parser.add_argument("--out", required=True, type=Path)
parser.add_argument("--scenes", default="")
parser.add_argument("--width", default="0")
parser.add_argument("--height", default="0")
parser.add_argument("--scale", default="1")
parser.add_argument("--font", default="")
parser.add_argument("--rtl", action="store_true")
parser.add_argument("--contrast", action="store_true")
parser.add_argument("--accent", default="")
parser.add_argument("--build", type=Path, default=Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) / "starlight-dev/preview")
args = parser.parse_args()
args.build.mkdir(parents=True, exist_ok=True)
subprocess.run(["qmake6", str(SOURCE / "preview.pro")], cwd=args.build, check=True, stdout=subprocess.DEVNULL)
subprocess.run(["make", "-s", "-j4"], cwd=args.build, check=True)
args.out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="starlight-preview-") as private:
    root = Path(private)
    for name in ("home", "config", "cache", "data", "run"):
        (root / name).mkdir(mode=0o700)
    if args.desktop == "plasma":
        colors = "BreezeDark" if args.scheme == "dark" else "BreezeLight"
        shutil.copy(f"/usr/share/color-schemes/{colors}.colors", root / "config/kdeglobals")
        with (root / "config/kdeglobals").open("a") as f:
            f.write("\n[KDE]\nwidgetStyle=Breeze\n[Icons]\nTheme=breeze%s\n" % ("-dark" if args.scheme == "dark" else ""))
    env = {k: os.environ[k] for k in ("PATH", "LANG") if k in os.environ}
    env.update(HOME=str(root / "home"), XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"),
               XDG_DATA_HOME=str(root / "data"), XDG_RUNTIME_DIR=str(root / "run"),
               XDG_DATA_DIRS="/usr/local/share:/usr/share", XDG_CURRENT_DESKTOP={"gnome": "GNOME", "plasma": "KDE", "generic": "Hyprland"}[args.desktop],
               DBUS_SESSION_BUS_ADDRESS="unix:path=" + str(root / "no-bus"),
               QT_FORCE_STDERR_LOGGING="1", QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software", QML_DISABLE_DISK_CACHE="1", QT_SCALE_FACTOR=args.scale,
               STARLIGHT_PREVIEW_DESKTOP=args.desktop, STARLIGHT_PREVIEW_SCHEME=args.scheme,
               STARLIGHT_PREVIEW_OUT=str(args.out.resolve()), STARLIGHT_PREVIEW_SCENES=args.scenes,
               STARLIGHT_PREVIEW_W=args.width, STARLIGHT_PREVIEW_H=args.height)
    if args.font: env["STARLIGHT_PREVIEW_FONT"] = args.font
    if args.rtl: env["STARLIGHT_PREVIEW_RTL"] = "1"
    if args.contrast: env["STARLIGHT_PREVIEW_CONTRAST"] = "1"
    if args.accent: env["STARLIGHT_PREVIEW_ACCENT"] = args.accent
    if args.desktop == "plasma":
        env["QT_QPA_PLATFORMTHEME"] = "kde"
    raise SystemExit(subprocess.run([str(args.build / "starlight-preview")], env=env, cwd=root, timeout=int(os.environ.get("PREVIEW_TIMEOUT","120"))).returncode)
