#!/usr/bin/env python3
"""Stage an exact local Flatpak source snapshot; print build commands without running them."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import stat
import subprocess
import tarfile
import tempfile

import yaml


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def clean_repository(repo):
    if git(repo, "status", "--porcelain", "--untracked-files=all", "--ignore-submodules=none"):
        raise RuntimeError(f"refusing dirty source checkout: {repo}")


def snapshot(repo, revision, destination, submodules, prefix=""):
    if Path(git(repo, "rev-parse", "--show-toplevel").decode().strip()).resolve() != repo.resolve():
        raise RuntimeError(f"submodule is not initialized: {repo}")
    clean_repository(repo)
    git(repo, "cat-file", "-e", f"{revision}^{{commit}}")
    destination.mkdir(parents=True, exist_ok=True)
    with subprocess.Popen(["git", "-C", str(repo), "archive", "--format=tar", revision],
                          stdout=subprocess.PIPE) as process:
        with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
            for member in archive:
                if any(part in {".git", ".worktrees"} for part in Path(member.name).parts):
                    continue
                path = Path(member.name)
                if ({".claude", ".codex", ".ssh", ".gnupg"}.intersection(path.parts)
                        or path.name in {".env", "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519", "credentials.json"}):
                    raise RuntimeError(f"private local state must not be packaged: {prefix}{member.name}")
                archive.extract(member, destination, filter="data")
        if process.wait():
            raise RuntimeError(f"git archive failed: {repo}")
    for entry in git(repo, "ls-tree", "-rz", revision).split(b"\0"):
        if not entry:
            continue
        metadata, raw_path = entry.split(b"\t", 1)
        mode, _, object_id = metadata.decode().split()
        if mode == "160000":
            relative = raw_path.decode()
            full_path = f"{prefix}{relative}"
            submodules[full_path] = object_id
            snapshot(repo / relative, object_id, destination / relative, submodules, full_path + "/")


def file_hash(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def value_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def source_hash(source):
    """Hash names, executable modes and bytes, independent of extraction timestamps."""
    digest = hashlib.sha256()
    for path in sorted(source.rglob("*")):
        mode = path.lstat().st_mode
        if path.is_symlink():
            content = "link:" + str(path.readlink())
        elif stat.S_ISREG(mode):
            content = "file:" + file_hash(path)
        else:
            continue
        digest.update(f"{path.relative_to(source).as_posix()}\0{stat.S_IMODE(mode):o}\0{content}\0".encode())
    return digest.hexdigest()


def modules(manifest):
    for module in manifest.get("modules", []):
        if not isinstance(module, dict):
            raise RuntimeError("external module files are not supported by this staging helper")
        yield module
        yield from modules(module)


def stage(repo, revision, output, workspace, build_dir=None):
    repo, output, workspace = repo.resolve(), output.resolve(), workspace.resolve()
    effective_build = (build_dir or workspace / "build-dir").resolve()
    for protected in (repo, output, workspace / "repo", workspace / ".flatpak-builder"):
        if effective_build.is_relative_to(protected) or protected.is_relative_to(effective_build):
            raise RuntimeError("force-clean build directory must not overlap source, candidate, export repo or builder state")
    if workspace.is_relative_to(effective_build):
        raise RuntimeError("force-clean build directory must not contain the workspace/cache")
    if output.is_relative_to(repo):
        raise RuntimeError("candidate output must be outside the source checkout")
    if output.exists():
        raise RuntimeError(f"candidate output already exists: {output}")
    clean_repository(repo)
    commit = git(repo, "rev-parse", "--verify", "--end-of-options", f"{revision}^{{commit}}").decode().strip()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".starlight-stage-", dir=output.parent) as temporary:
        candidate = Path(temporary)
        source = candidate / "source"
        submodules = {}
        snapshot(repo, commit, source, submodules)
        canonical = source / "flatpak/com.starlight.Starlight.yaml"
        manifest = yaml.safe_load(canonical.read_text())
        all_modules = list(modules(manifest))
        apps = [module for module in all_modules if module.get("name") == "starlight"]
        if len(apps) != 1 or len(apps[0].get("sources", [])) != 1 or apps[0]["sources"][0].get("type") != "git":
            raise RuntimeError("expected one starlight module with one canonical Git source")
        dependencies = [module for module in all_modules if module is not apps[0]]
        patches = {}
        for module in all_modules:
            for item in module.get("sources", []):
                if "path" not in item:
                    continue
                relative = Path(item["path"])
                original = (canonical.parent / relative).resolve()
                if relative.is_absolute() or not original.is_relative_to(canonical.parent):
                    raise RuntimeError(f"local manifest source escapes its directory: {relative}")
                copied = (candidate / relative).resolve()
                if (not copied.is_relative_to(candidate)
                        or copied.relative_to(candidate).parts[0] in {"source", "manifest.yaml", "provenance.json"}):
                    raise RuntimeError(f"local manifest source uses a reserved candidate path: {relative}")
                copied.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(original, copied)
                patches[relative.as_posix()] = file_hash(copied)
        dependency_hash = value_hash({
            "platform": {key: manifest[key] for key in
                         ("runtime", "runtime-version", "sdk", "sdk-extensions", "base", "base-version", "build-options")
                         if key in manifest},
            "modules": dependencies, "local_sources_sha256": patches,
        })
        apps[0]["sources"] = [{"type": "dir", "path": str(output / "source")}]
        apps[0].setdefault("build-options", {}).setdefault("env", {})["STARLIGHT_BUILD_REVISION"] = commit
        local_manifest = candidate / "manifest.yaml"
        local_manifest.write_text(yaml.safe_dump(manifest, sort_keys=False))
        build_command = ["flatpak-builder", "--user", "--force-clean",
                         f"--state-dir={workspace / '.flatpak-builder'}", f"--repo={workspace / 'repo'}",
                         str(effective_build), "manifest.yaml"]
        result = {
            "revision": commit, "submodules": submodules, "source_sha256": source_hash(source),
            "canonical_manifest_sha256": file_hash(canonical), "manifest_sha256": file_hash(local_manifest),
            "dependencies_sha256": dependency_hash, "patches_sha256": patches,
            "dependencies": [{"name": item["name"], "definition_sha256": value_hash(item),
                              "sources": item.get("sources", [])} for item in dependencies],
            "build_command": shlex.join(build_command),
            "build_working_directory": str(output),
            "bundle_command": shlex.join(["flatpak", "build-bundle", str(workspace / "repo"),
                                          str(output / f"Starlight-{commit[:12]}.flatpak"), manifest["app-id"]]),
        }
        (candidate / "provenance.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        # A concurrent edit must not turn this into a claimed clean candidate.
        clean_repository(repo)
        candidate.rename(output)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--revision", default="HEAD")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, required=True, help="existing Flatpak workspace/cache root")
    parser.add_argument("--build-dir", type=Path, help="existing build directory (default: WORKSPACE/build-dir)")
    args = parser.parse_args()
    try:
        result = stage(args.repo, args.revision, args.output, args.workspace, args.build_dir)
    except (OSError, RuntimeError, subprocess.CalledProcessError, yaml.YAMLError) as error:
        parser.exit(1, f"ERROR: {error}\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
