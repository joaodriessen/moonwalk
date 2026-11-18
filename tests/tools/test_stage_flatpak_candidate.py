"""Prove exact source/submodule staging and refusal of mutable package inputs."""
import importlib.util
import json
import shlex
from pathlib import Path
import subprocess
import tempfile
import unittest

import yaml

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/stage-flatpak-candidate.py"
SPEC = importlib.util.spec_from_file_location("stage_flatpak", SCRIPT)
staging = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(staging)


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), "-c", "protocol.file.allow=always", *args],
                                   stderr=subprocess.DEVNULL).decode().strip()


def initialize(path):
    path.mkdir()
    git(path, "init", "-q")
    git(path, "config", "user.name", "Fixture")
    git(path, "config", "user.email", "fixture@example.invalid")


def commit(repo):
    git(repo, "add", ".")
    git(repo, "commit", "-qm", "fixture")
    return git(repo, "rev-parse", "HEAD")


class CandidateStagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo = self.root / "app"
        leaf, dependency = self.root / "leaf", self.root / "dependency"
        for path in (self.repo, leaf, dependency):
            initialize(path)
        (leaf / "nested.cpp").write_text("nested pinned source\n")
        self.leaf_revision = commit(leaf)
        (dependency / "engine.cpp").write_text("pinned engine\n")
        git(dependency, "submodule", "add", "-q", str(leaf), "nested")
        self.dependency_revision = commit(dependency)
        git(self.repo, "submodule", "add", "-q", str(dependency), "engine")
        git(self.repo, "submodule", "update", "--init", "--recursive")
        (self.repo / ".gitignore").write_text("private-profile\n.worktrees/\n")
        (self.repo / "app.cpp").write_text("tracked source\n")
        (self.repo / "configure").write_text("#!/bin/sh\n")
        (self.repo / "configure").chmod(0o755)
        (self.repo / "flatpak/patches").mkdir(parents=True)
        (self.repo / "flatpak/patches/input.patch").write_text("patch bytes\n")
        manifest = {
            "app-id": "com.starlight.Starlight",
            "modules": [
                {"name": "sdl", "sources": [{"type": "git", "url": "https://example.invalid/sdl.git", "commit": "b" * 40},
                                            {"type": "patch", "path": "patches/input.patch"}]},
                {"name": "starlight", "build-options": {"env": {"OTHER": "keep"}},
                 "sources": [{"type": "git", "url": "https://example.invalid/app.git", "commit": "a" * 40}]},
            ],
        }
        (self.repo / "flatpak/com.starlight.Starlight.yaml").write_text(yaml.safe_dump(manifest))
        self.revision = commit(self.repo)
        self.output = self.root / "candidate"

    def stage(self, revision=None, output=None):
        return staging.stage(self.repo, revision or self.revision, output or self.output, self.root / "warm")

    def test_snapshot_excludes_private_files_and_preserves_recursive_gitlinks(self):
        (self.repo / "private-profile").write_text("never package")
        (self.repo / ".worktrees").mkdir()
        (self.repo / ".worktrees/private").write_text("never package")
        result = self.stage()
        source = self.output / "source"
        self.assertEqual((source / "app.cpp").read_text(), "tracked source\n")
        self.assertEqual((source / "engine/engine.cpp").read_text(), "pinned engine\n")
        self.assertEqual((source / "engine/nested/nested.cpp").read_text(), "nested pinned source\n")
        self.assertTrue((source / "configure").stat().st_mode & 0o111)
        self.assertFalse(any(path.name in {".git", ".worktrees", "private-profile"} for path in source.rglob("*")))
        self.assertEqual(result["submodules"], {"engine": self.dependency_revision,
                                               "engine/nested": self.leaf_revision})

    def test_manifest_revision_patches_and_hashes_match_staged_inputs(self):
        result = self.stage()
        manifest_path = self.output / "manifest.yaml"
        manifest = yaml.safe_load(manifest_path.read_text())
        app = manifest["modules"][-1]
        self.assertEqual(app["sources"], [{"type": "dir", "path": str(self.output / "source")}])
        self.assertEqual(app["build-options"]["env"], {"OTHER": "keep", "STARLIGHT_BUILD_REVISION": self.revision})
        self.assertEqual(result["build_working_directory"], str(self.output))
        self.assertEqual(shlex.split(result["build_command"])[-1], "manifest.yaml")
        patch = self.output / "patches/input.patch"
        self.assertEqual(patch.read_text(), "patch bytes\n")
        self.assertEqual(result["manifest_sha256"], staging.file_hash(manifest_path))
        self.assertEqual(result["patches_sha256"]["patches/input.patch"], staging.file_hash(patch))
        self.assertEqual(result["source_sha256"], staging.source_hash(self.output / "source"))
        self.assertEqual(json.loads((self.output / "provenance.json").read_text()), result)
        second = self.stage(output=self.root / "candidate-two")
        self.assertEqual(result["source_sha256"], second["source_sha256"])
        self.assertEqual(result["dependencies_sha256"], second["dependencies_sha256"])
        self.assertFalse((self.root / "warm").exists(), "staging must not invoke the builder")

    def test_requested_revision_uses_its_pinned_submodule_not_current_checkout(self):
        engine = self.repo / "engine"
        git(engine, "config", "user.name", "Fixture")
        git(engine, "config", "user.email", "fixture@example.invalid")
        (engine / "engine.cpp").write_text("new engine\n")
        commit(engine)
        (self.repo / "app.cpp").write_text("new app\n")
        commit(self.repo)
        self.stage(self.revision)
        self.assertEqual((self.output / "source/app.cpp").read_text(), "tracked source\n")
        self.assertEqual((self.output / "source/engine/engine.cpp").read_text(), "pinned engine\n")

    def test_dirty_and_untracked_sources_are_refused_before_output(self):
        for name in ("app.cpp", "untracked-secret"):
            with self.subTest(name=name):
                path = self.repo / name
                original = path.read_text() if path.exists() else None
                path.write_text("uncommitted")
                with self.assertRaisesRegex(RuntimeError, "dirty"):
                    self.stage()
                self.assertFalse(self.output.exists())
                path.write_text(original) if original is not None else path.unlink()

    def test_dirty_submodule_and_existing_output_are_refused(self):
        (self.repo / "engine/engine.cpp").write_text("dirty engine")
        with self.assertRaisesRegex(RuntimeError, "dirty"):
            self.stage()
        git(self.repo / "engine", "checkout", "--", "engine.cpp")
        self.output.mkdir()
        (self.output / "keep").write_text("keep")
        with self.assertRaisesRegex(RuntimeError, "already exists"):
            self.stage()
        self.assertEqual((self.output / "keep").read_text(), "keep")

    def test_uninitialized_submodule_is_refused_without_publishing_a_partial_candidate(self):
        git(self.repo, "submodule", "deinit", "-f", "engine")
        with self.assertRaisesRegex(RuntimeError, "not initialized"):
            self.stage()
        self.assertFalse(self.output.exists())

    def test_changed_dependency_patch_changes_dependency_hash(self):
        original = self.stage()
        (self.repo / "flatpak/patches/input.patch").write_text("different patch\n")
        revision = commit(self.repo)
        changed = self.stage(revision=revision, output=self.root / "changed")
        self.assertNotEqual(original["dependencies_sha256"], changed["dependencies_sha256"])

    def test_manifest_inputs_cannot_overwrite_candidate_source_or_metadata(self):
        manifest_path = self.repo / "flatpak/com.starlight.Starlight.yaml"
        original = yaml.safe_load(manifest_path.read_text())
        for relative in ("source/app.cpp", "manifest.yaml", "provenance.json"):
            with self.subTest(relative=relative):
                path = self.repo / "flatpak" / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("must not overwrite staged files\n")
                manifest = json.loads(json.dumps(original))
                manifest["modules"][0]["sources"][1]["path"] = relative
                manifest_path.write_text(yaml.safe_dump(manifest))
                revision = commit(self.repo)
                with self.assertRaisesRegex(RuntimeError, "reserved candidate path"):
                    self.stage(revision=revision)
                self.assertFalse(self.output.exists())

    def test_force_clean_build_directory_cannot_overlap_source_or_candidate(self):
        for directory in (self.repo, self.repo / "build", self.root,
                          self.output, self.output / "source", self.root / "warm/repo",
                          self.root / "warm/.flatpak-builder"):
            with self.subTest(directory=directory):
                with self.assertRaisesRegex(RuntimeError, "build directory"):
                    staging.stage(self.repo, self.revision, self.output, self.root / "warm", directory)
                self.assertFalse(self.output.exists())
                self.assertEqual((self.repo / "app.cpp").read_text(), "tracked source\n")

    def test_committed_private_state_is_rejected_not_silently_packaged(self):
        private = self.repo / ".codex/session.jsonl"
        private.parent.mkdir()
        private.write_text("synthetic private state\n")
        revision = commit(self.repo)
        with self.assertRaisesRegex(RuntimeError, "private local state"):
            self.stage(revision=revision)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
