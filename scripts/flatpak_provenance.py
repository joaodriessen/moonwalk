"""Content fingerprints shared by Flatpak staging and bundle verification."""
import hashlib
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
