#!/usr/bin/env python3
"""Reject common private material from the exact staged Git tree."""
from pathlib import PurePosixPath
import re
import struct
import subprocess
import sys

CREDENTIAL = re.compile(rb"\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16}|"
                        rb"sk-(?:proj-)?[A-Za-z0-9_-]{30,}|xox[baprs]-[A-Za-z0-9-]{15,})\b")
PRIVATE_KEY = re.compile(rb"-----BEGIN [A-Z ]*PRIVATE KEY-----\r?\n[A-Za-z0-9+/=]{24,}")
PRIVATE_PATH = re.compile(rb"(?i)(?:/home/|/Users/|[A-Z]:[/\\]Users[/\\])([\w.-]+)[/\\]")
EXAMPLE_HOME = {b"builder", b"example", b"someone", b"alice", b"bob"}
PRIVATE_FILES = re.compile(r"(?i)(?:\.(?:env|pfx|p12|key|etl|pcapng?|log|sqlite3?)$|^\.env\.|"
                           r"^(?:credentials\.json|known_hosts|id_(?:rsa|ecdsa|ed25519|dsa))$)")
PRIVATE_DIRS = {".claude", ".codex", ".ssh", ".gnupg", ".superpowers"}
PNG = b"\x89PNG\r\n\x1a\n"
PNG_PRIVATE_CHUNKS = {b"eXIf", b"tEXt", b"iTXt", b"zTXt"}


def check_blob(data):
    if data.startswith(PNG):
        offset = len(PNG)
        while offset + 12 <= len(data):
            length, kind = struct.unpack_from(">I4s", data, offset)
            offset += 12 + length
            if kind in PNG_PRIVATE_CHUNKS:
                yield "PNG text or EXIF metadata"
            if kind == b"IEND":
                if offset != len(data):
                    yield "malformed PNG or data after IEND"
                return
        yield "invalid PNG"
        return
    if CREDENTIAL.search(data) or PRIVATE_KEY.search(data):
        yield "possible credential"
    normalized = data.replace(b"\\\\", b"\\")
    if any(match[1].lower() not in EXAMPLE_HOME for match in PRIVATE_PATH.finditer(normalized)):
        yield "personal home path"


def main():
    git = ["git", "-C", str(PurePosixPath(__file__).parent.parent)]
    entries = subprocess.check_output(git + ["ls-files", "--stage", "-z"]).split(b"\0")
    errors = 0

    def report(reason, path):
        nonlocal errors
        errors += 1
        # Avoid repeating sensitive names in hosted workflow logs.
        where = f": {path}" if "--show-paths" in sys.argv else ""
        print(f"Privacy check: {reason}{where}", file=sys.stderr)

    with subprocess.Popen(git + ["cat-file", "--batch"], stdin=subprocess.PIPE, stdout=subprocess.PIPE) as batch:
        for entry in filter(None, entries):
            stat, raw_path = entry.split(b"\t", 1)
            mode, oid, stage = stat.split()
            path = PurePosixPath(raw_path.decode(errors="surrogateescape"))
            if PRIVATE_DIRS.intersection(path.parts) or PRIVATE_FILES.search(path.name):
                report("private file or directory", path)
            if stage != b"0":
                report("unresolved index conflict", path)
                continue
            if mode == b"160000":
                continue  # Reviewed public gitlinks are separate repositories.
            batch.stdin.write(oid + b"\n")
            batch.stdin.flush()
            _, kind, length = batch.stdout.readline().split()
            assert kind == b"blob"
            data = batch.stdout.read(int(length))
            assert batch.stdout.read(1) == b"\n"
            for reason in check_blob(data):
                report(reason, path)
        batch.stdin.close()
        if batch.wait() != 0:
            return 1
    if errors:
        print(f"{errors} privacy findings. Review locally with --show-paths.", file=sys.stderr)
        return 1
    print("Staged privacy checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
