#!/usr/bin/env python3
"""Offline checks for maintained docs and release identity, not release certification."""
import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET


def product_version(project):
    base = re.search(r"^VERSION\s*=\s*(\d+\.\d+\.\d+)\s*$", project, re.M)
    suffix = re.search(r"STARLIGHT_VERSION_STR=.*?\$\$VERSION([\w.-]*)", project)
    if not base or not suffix:
        raise ValueError("cannot read Moonwalk product version from app/app.pro")
    return base[1] + suffix[1]


def check_tracked_paths(paths):
    """Catch common accidental private files, not secrets embedded in source/history."""
    private_dirs = {".claude", ".codex", ".ssh", ".gnupg"}
    private_files = {".env", "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519", "credentials.json"}
    errors = []
    for name in paths:
        path = PurePosixPath(name)
        if private_dirs.intersection(path.parts) or path.name in private_files:
            errors.append(f"private local state must not be tracked: {name}")
    return errors


def check(repo):
    errors = []
    # Historical design/validation records are not current instructions.
    documents = [repo / f for f in ("README.md", "SECURITY.md", "CONTRIBUTING.md", "CHANGELOG.md")]
    documents += sorted((repo / "docs").glob("*.md"))
    documents += sorted((repo / "docs/guides").glob("*.md"))
    for path in documents:
        if not path.is_file():
            errors.append(f"missing document: {path.relative_to(repo)}")
            continue
        text = path.read_text()
        targets = re.findall(r'!?\[[^\]]*\]\(([^\s)]+)(?:\s+[^)]*)?\)', text)
        targets += re.findall(r'\b(?:src|srcset)="([^"]+)"', text)
        for target in targets:
            url = urlsplit(target.strip("<>"))
            if url.scheme or url.netloc or not url.path:
                continue
            linked = (path.parent / unquote(url.path)).resolve()
            if not linked.is_relative_to(repo) or not linked.exists():
                errors.append(f"broken local link in {path.relative_to(repo)}: {target}")
    try:
        version = product_version((repo / "app/app.pro").read_text())
        metadata = ET.parse(repo / "app/deploy/linux/com.starlight.Starlight.appdata.xml").getroot()
        latest = metadata.find("releases/release")
        if latest is None or latest.get("version") != version:
            errors.append(f"AppStream latest release must match product version {version}")
        if not re.search(r"^## " + re.escape(version) + r"(?:\s|,|$)",
                         (repo / "CHANGELOG.md").read_text(), re.M):
            errors.append(f"changelog has no release entry for {version}")
    except (ValueError, OSError, ET.ParseError) as error:
        errors.append(str(error))
    return errors


def check_flatpak_pin(repo):
    # PyYAML is needed only for this opt-in packaging check.
    import yaml
    try:
        manifest = yaml.safe_load((repo / "flatpak/com.starlight.Starlight.yaml").read_text())
    except yaml.YAMLError:
        return ["Flatpak manifest is not valid YAML"]
    if (not isinstance(manifest, dict) or not isinstance(manifest.get("modules"), list)
            or not all(isinstance(m, dict) for m in manifest["modules"])):
        return ["Flatpak manifest needs an inline module list"]
    apps = [m for m in manifest["modules"] if m.get("name") == "starlight"]
    if len(apps) != 1:
        return ["Flatpak manifest needs exactly one starlight module"]
    sources = apps[0].get("sources")
    if (not isinstance(sources, list) or len(sources) != 1
            or not isinstance(sources[0], dict) or sources[0].get("type") != "git"):
        return ["Flatpak app needs exactly one canonical Git source"]
    pin = sources[0].get("commit", "")
    if not isinstance(pin, str) or not re.fullmatch(r"[0-9a-f]{40}", pin):
        return ["Flatpak app source needs an exact commit"]
    # Include working-tree edits and gitlinks, but not the self-referential manifest.
    inputs = ["app", ":(top)*.pro", ":(top)*.pri", ".gitmodules", "LICENSE", "AntiHooking",
              "config.tests", "qmdnsengine", "h264bitstream", "moonlight-common-c"]
    untracked = subprocess.check_output(["git", "-C", str(repo), "ls-files", "--others", "--exclude-standard", "-z", "--", *inputs],
                                        text=True).rstrip("\0")
    if untracked:
        return ["Untracked app/build inputs cannot be verified against the Flatpak pin: " + ", ".join(untracked.split("\0"))]
    changed = subprocess.check_output(["git", "-C", str(repo), "diff", "--ignore-submodules=none", "--name-only", pin, "--", *inputs],
                                      text=True).splitlines()
    return ([f"Flatpak pin {pin[:12]} differs from current app/build inputs: " + ", ".join(changed)]
            if changed else [])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-flatpak-pin", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    errors = check(repo)
    try:
        tracked = subprocess.check_output(["git", "-C", str(repo), "ls-files", "-z"], text=True)
        errors += check_tracked_paths(tracked.split("\0"))
    except (OSError, subprocess.CalledProcessError) as error:
        errors.append(f"cannot check tracked private paths: {error}")
    if args.check_flatpak_pin:
        try:
            errors += check_flatpak_pin(repo)
        except (ImportError, OSError, ValueError, KeyError, StopIteration, subprocess.CalledProcessError) as error:
            errors.append(f"cannot verify Flatpak pin: {error}")
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    if not errors:
        print("Repository links, release identity and private-path guard: PASS (not runtime or publication certification)")
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())
