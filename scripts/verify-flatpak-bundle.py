#!/usr/bin/env python3
"""Check a Moonwalk bundle against its staged source without installing it."""
import argparse
import configparser
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

import yaml

from flatpak_provenance import file_hash, verify_stage


def run(*command):
    return subprocess.check_output(command, text=True, timeout=180).strip()


def require(actual, expected, field):
    if actual != expected:
        raise ValueError(f"{field} differs from the staged candidate")


def read_metadata(path):
    metadata = configparser.ConfigParser(interpolation=None)
    metadata.optionxform = str
    metadata.read_string(path.read_text())
    return metadata


def sandbox_policy(metadata):
    policy = {section: dict(metadata[section]) for section in metadata.sections()
              if section in {"Context", "Session Bus Policy", "System Bus Policy", "Environment"}
              or section.startswith("Policy ")}
    context = policy.get("Context", {})
    for key, value in context.items():
        context[key] = set(filter(None, value.split(";")))
    sockets = context.get("sockets", set())
    if "fallback-x11" in sockets:
        # Flatpak 1.14 writes both flags. Fallback takes precedence over x11.
        sockets.discard("x11")
    if metadata["Application"]["runtime"].startswith("org.kde.Platform/"):
        # Newer Flatpak adds these permissions for the KDE runtime. Treat the
        # additive defaults alike across Builder and verifier versions.
        context.get("filesystems", set()).discard("xdg-config/kdeglobals:ro")
        bus = policy.get("Session Bus Policy", {})
        for name in ("com.canonical.AppMenu.Registrar", "org.kde.kconfig.notify", "org.kde.KGlobalSettings"):
            if bus.get(name) == "talk":
                del bus[name]
    policy["Context"] = {key: values for key, values in context.items() if values}
    return {section: values for section, values in policy.items() if values}


def verify_sandbox(metadata, manifest, app_id, runtime, scratch):
    (scratch / "files").mkdir(parents=True)
    expected = scratch / "metadata"
    expected.write_text(f"[Application]\nname={app_id}\nruntime={runtime}\n")
    run("flatpak", "build-finish", "--no-exports", *manifest.get("finish-args", []), scratch)
    require(sandbox_policy(metadata), sandbox_policy(read_metadata(expected)), "sandbox permissions or environment")


def verify(candidate, bundle):
    manifest_path = candidate / "manifest.yaml"
    provenance = verify_stage(candidate)
    source = candidate / "source"
    revision = provenance["revision"]

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

        metadata = read_metadata(app / "metadata")
        require(metadata["Application"]["name"], app_id, "application ID")
        require(metadata["Application"]["command"], command, "application command")
        require(metadata["Application"]["runtime"], runtime, "runtime ref")
        verify_sandbox(metadata, manifest, app_id, runtime, root / "expected-permissions")

        share = app / "files/share"
        metainfo = share / "metainfo" / f"{app_id}.metainfo.xml"
        installed_data = ET.parse(metainfo if metainfo.is_file() else share / "metainfo" / appdata)
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
            "sandbox_permissions": {
                section: dict(metadata[section])
                for section in ("Context", "Session Bus Policy", "System Bus Policy")
                if metadata.has_section(section)
            },
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
