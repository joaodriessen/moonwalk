"""Content fingerprints shared by Flatpak staging and bundle verification."""
import hashlib
import json
import re
import stat


def file_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def source_hash(source):
    """Hash names, executable modes and bytes independently of timestamps."""
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


def verify_stage(candidate):
    """Read the provenance only after checking the staged files it identifies."""
    provenance = json.loads((candidate / "provenance.json").read_text())
    if not re.fullmatch(r"[0-9a-f]{40}", provenance["revision"]):
        raise ValueError("staged source revision must be a full Git commit")
    if file_hash(candidate / "manifest.yaml") != provenance["manifest_sha256"]:
        raise ValueError("staged manifest checksum differs from the staged candidate")
    source = candidate / "source"
    if not source.is_dir() or source_hash(source) != provenance["source_sha256"]:
        raise ValueError("staged source fingerprint differs from the staged candidate")
    for name, checksum in provenance["patches_sha256"].items():
        patch = (candidate / name).resolve()
        if not patch.is_relative_to(candidate.resolve()):
            raise ValueError("staged patch is outside the candidate directory")
        if file_hash(patch) != checksum:
            raise ValueError("staged patch checksum differs from the staged candidate")
    return provenance
