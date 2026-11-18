#!/usr/bin/env python3
"""Select cheap docs checks, QML checks, or a full app build; unknown changes build."""
import os
from pathlib import Path
import subprocess


def scope(paths, full=False):
    if full:
        return True, True
    code = [p for p in paths if not (p.startswith("docs/") or p.endswith(".md"))]
    # QML/JS changes still run all host tests and the production QML compiler.
    # Everything else includes resources, metadata, nested gitlinks and CI itself.
    return bool(code), any(Path(p).suffix not in {".qml", ".js"} for p in code)


def main():
    event = os.environ.get("EVENT_NAME", "")
    full = os.environ.get("RELEASE_CHECK") == "true"
    if event == "workflow_dispatch" and not full:
        checks, app = True, False
    elif full:
        checks, app = True, True
    else:
        base = os.environ.get("BASE_REF", "")
        try:
            # Resolve data as a revision, never as a command-line option.
            revision = subprocess.check_output(
                ["git", "rev-parse", "--verify", "--end-of-options", base + "^{commit}"],
                text=True, stderr=subprocess.DEVNULL).strip()
            paths = subprocess.check_output(["git", "diff", "--name-only", "-z", revision, "HEAD"],
                                            text=True).rstrip("\0").split("\0")
            checks, app = scope([p for p in paths if p])
        except subprocess.CalledProcessError:
            checks, app = True, True
    print(f"checks={str(checks).lower()}\napp={str(app).lower()}")


if __name__ == "__main__":
    main()
