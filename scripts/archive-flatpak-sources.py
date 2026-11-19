#!/usr/bin/env python3
"""Make a portable source archive from a verified stage and Flatpak Builder export."""
import argparse
import gzip
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile

import yaml

from flatpak_provenance import file_hash, source_hash, verify_stage


BARE_CONFIG = b"[core]\n\trepositoryformatversion = 0\n\tbare = true\n"


def normalized_header(member):
    if member.type not in (tarfile.REGTYPE, tarfile.DIRTYPE, tarfile.SYMTYPE, tarfile.LNKTYPE):
        raise ValueError("special file in source archive")
    member.uid = member.gid = member.mtime = 0
    member.uname = member.gname = ""
    return member


def add_bytes(archive, name, content):
    member = tarfile.TarInfo(name)
    member.size = len(content)
    archive.addfile(member, io.BytesIO(content))


def add_git_mirror(archive, mirror):
    if not mirror.is_dir() or mirror.is_symlink():
        raise ValueError("invalid Git mirror directory")
    for name in ("alternates", "http-alternates"):
        if (mirror / "objects/info" / name).exists():
            raise ValueError("Git mirror refers to an external object store")
    root = f"git/{mirror.name}"
    add_bytes(archive, f"{root}/config", BARE_CONFIG)
    for name in ("HEAD", "packed-refs", "shallow", "objects", "refs", "lfs/objects"):
        entry = mirror / name
        if entry.is_symlink() or any(path.is_symlink() for path in entry.rglob("*")):
            raise ValueError("unexpected symlink in a Git mirror")
        if entry.exists():
            archive.add(entry, f"{root}/{name}", filter=normalized_header)


def add_downloads(archive, downloads, dependencies):
    checksums = {source["sha256"] for module in dependencies for source in module["sources"]
                 if source["type"] in {"archive", "file"} and "url" in source}
    for checksum in sorted(checksums):
        if not re.fullmatch(r"[0-9a-f]{64}", checksum):
            raise ValueError("invalid source download checksum")
        directory = downloads / checksum
        if directory.is_symlink():
            raise ValueError("unexpected symlink in source downloads")
        files = sorted(directory.iterdir())
        if not files:
            raise ValueError("export is missing a pinned download")
        for path in files:
            if path.is_symlink() or not path.is_file() or file_hash(path) != checksum:
                raise ValueError("source download checksum differs from the staged candidate")
            archive.add(path, f"downloads/{checksum}/{path.name}", filter=normalized_header)


def archive_sources(candidate, repo, output):
    provenance = verify_stage(candidate)
    manifest = yaml.safe_load((candidate / "manifest.yaml").read_text())
    app = manifest["app-id"]
    arch = subprocess.check_output(["flatpak", "--default-arch"], text=True, timeout=10).strip()
    ref = f"runtime/{app}.Sources/{arch}/{manifest.get('branch', 'master')}"
    if output.exists():
        raise FileExistsError("source archive already exists")
    with tempfile.TemporaryDirectory(prefix=".moonwalk-sources-", dir=output.parent) as work:
        work = Path(work)
        tree = work / "checkout"
        subprocess.run(["ostree", f"--repo={repo}", "checkout", "--user-mode", ref, tree],
                       check=True, timeout=180)
        files = tree / "files"
        recipes = files / "manifest"
        source = recipes / "source"
        if file_hash(recipes / f"{app}.json") != provenance["manifest_sha256"]:
            raise ValueError("exported source manifest differs from the staged candidate")
        if not source.is_dir() or source_hash(source) != provenance["source_sha256"]:
            raise ValueError("exported source fingerprint differs from the staged candidate")
        for name, checksum in provenance["patches_sha256"].items():
            patch = recipes / name
            if not patch.resolve().is_relative_to(recipes) or file_hash(patch) != checksum:
                raise ValueError("exported patch differs from the staged candidate")

        bundle = work / "sources.tar.gz"
        with bundle.open("wb") as stream, gzip.GzipFile(filename="", mode="wb", fileobj=stream,
                                                       compresslevel=6, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w|") as archive:
                archive.add(recipes / f"{app}.json", "manifest/manifest.yaml", filter=normalized_header)
                archive.add(source, "manifest/source", filter=normalized_header)
                for name in sorted(provenance["patches_sha256"]):
                    archive.add(recipes / name, f"manifest/{name}", filter=normalized_header)
                add_downloads(archive, files / "downloads", provenance["dependencies"])
                for mirror in sorted((files / "git").iterdir()):
                    add_git_mirror(archive, mirror)
                receipt = {key: provenance[key] for key in
                           ("revision", "source_sha256", "manifest_sha256", "patches_sha256", "submodules")}
                add_bytes(archive, "provenance.json", (json.dumps(receipt, sort_keys=True, indent=2) + "\n").encode())
        report = {"source_revision": provenance["revision"], "size_bytes": bundle.stat().st_size,
                  "sha256": file_hash(bundle)}
        os.link(bundle, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path, help="directory generated by stage-flatpak-candidate.py")
    parser.add_argument("repo", type=Path, help="OSTree export from a build with --bundle-sources")
    parser.add_argument("output", type=Path, help="new .tar.gz file")
    args = parser.parse_args()
    try:
        report = archive_sources(args.candidate.resolve(), args.repo.resolve(), args.output)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, yaml.YAMLError) as error:
        parser.exit(1, f"Source archive failed: {error}\n")
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
