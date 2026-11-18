#!/usr/bin/env python3
"""Check a Moonwalk bundle against its staged source without installing it."""
import argparse
import configparser
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

import yaml

from flatpak_provenance import file_hash, source_hash


def run(*command):
    return subprocess.check_output(command, text=True, timeout=180).strip()


def require(actual, expected, field):
    if actual != expected:
        raise ValueError(f"{field} differs from the staged candidate")


def verify(candidate, bundle):
    manifest_path = candidate / "manifest.yaml"
    provenance = json.loads((candidate / "provenance.json").read_text())
    require(file_hash(manifest_path), provenance["manifest_sha256"], "staged manifest checksum")
    source = candidate / "source"
    require(source_hash(source), provenance["source_sha256"], "staged source fingerprint")
    for name, checksum in provenance["patches_sha256"].items():
        patch = (candidate / name).resolve()
        if not patch.is_relative_to(candidate.resolve()):
            raise ValueError("staged patch is outside the candidate directory")
        require(file_hash(patch), checksum, "staged patch checksum")
    revision = provenance["revision"]
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("staged source revision must be a full Git commit")

    manifest = yaml.safe_load(manifest_path.read_text())
    app_id = manifest["app-id"]
    command = manifest["command"]
    arch = run("flatpak", "--default-arch")
    runtime = f"{manifest['runtime']}/{arch}/{manifest['runtime-version']}"
    app_ref = f"app/{app_id}/{arch}/{manifest.get('branch', 'master')}"
    appdata = f"{app_id}.appdata.xml"
    version = ET.parse(source / "app/deploy/linux" / appdata).find("releases/release").attrib["version"]
    engine_version = (source / "app/version.txt").read_text().strip()

    with tempfile.TemporaryDirectory(prefix="moonwalk-bundle-") as temporary:
        root = Path(temporary)
        checked_bundle = root / "candidate.flatpak"
        # Inspect and hash the same bytes even if the original is replaced.
        shutil.copyfile(bundle, checked_bundle)
        repo = root / "repo"
        ostree = ("ostree", f"--repo={repo}")
        run(*ostree, "init", "--mode=archive")
        run("flatpak", "build-import-bundle", repo, checked_bundle)
        require(run(*ostree, "refs").splitlines(), [app_ref], "bundle ref")
        commit = run(*ostree, "rev-parse", app_ref)
        app = root / "app"
        run(*ostree, "checkout", "--user-mode", app_ref, app)

        metadata = configparser.ConfigParser(interpolation=None)
        metadata.read_string((app / "metadata").read_text())
        require(metadata["Application"]["name"], app_id, "application ID")
        require(metadata["Application"]["command"], command, "application command")
        require(metadata["Application"]["runtime"], runtime, "runtime ref")

        share = app / "files/share"
        installed_data = ET.parse(share / "metainfo" / appdata)
        require(installed_data.findtext("id"), app_id, "AppStream application ID")
        require(installed_data.find("releases/release").attrib["version"], version, "AppStream version")
        if not (share / "applications" / f"{app_id}.desktop").is_file():
            raise ValueError("bundle is missing its desktop launcher")
        require(file_hash(share / "licenses/moonwalk/moonwalk/LICENSE"), file_hash(source / "LICENSE"),
                "installed GPL notice")
        for module in manifest["modules"]:
            for notice in module.get("license-files", []):
                installed = share / "licenses" / app_id / module["name"] / Path(notice).name
                if not installed.is_file():
                    raise ValueError(f"bundle is missing a declared license file for {module['name']}")

        output = run(
            "flatpak", "build", "--runtime", "--readonly", "--unshare=network", "--unshare=ipc",
            "--nofilesystem=host:reset", "--nodevice=all", "--nosocket=x11", "--nosocket=fallback-x11",
            "--nosocket=wayland", "--nosocket=pulseaudio", "--nosocket=session-bus", "--nosocket=system-bus",
            "--env=QT_QPA_PLATFORM=offscreen", "--env=XDG_CONFIG_HOME=/tmp/moonwalk-verify/config",
            "--env=XDG_CACHE_HOME=/tmp/moonwalk-verify/cache", "--env=XDG_DATA_HOME=/tmp/moonwalk-verify/data",
            app, f"/app/bin/{command}", "--version",
        )
        require(output.splitlines(), [f"Moonwalk {version}", f"Source: {revision}",
                                      f"(moonlight-qt engine {engine_version})"], "packaged binary identity")
        return {
            "app_ref": app_ref,
            "commit": commit,
            "engine_version": engine_version,
            "license_files": sorted(path.relative_to(share).as_posix()
                                    for path in (share / "licenses").rglob("*") if path.is_file()),
            "product_version": version,
            "runtime_ref": runtime,
            "runtime_commit": run("flatpak", "info", "--show-commit", runtime),
            "sha256": file_hash(checked_bundle),
            "size_bytes": checked_bundle.stat().st_size,
            "source_revision": revision,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path, help="directory generated by stage-flatpak-candidate.py")
    parser.add_argument("bundle", type=Path, help="the built .flatpak file")
    args = parser.parse_args()
    try:
        result = verify(args.candidate, args.bundle)
    except (OSError, ValueError, KeyError, AttributeError, configparser.Error,
            subprocess.SubprocessError, ET.ParseError, yaml.YAMLError) as error:
        parser.exit(1, f"Bundle verification failed: {error}\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
