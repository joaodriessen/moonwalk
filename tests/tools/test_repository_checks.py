"""Release drift and missed-build regressions, using private synthetic files."""
import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


repository = load("check-repository")
ci = load("ci-scope")


class RepositoryChecks(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.repo = Path(self.tmp.name).resolve()
        for name in ("README.md", "SECURITY.md", "CONTRIBUTING.md"):
            (self.repo / name).write_text("[Guide](docs/guide.md)\n")
        (self.repo / "docs").mkdir()
        (self.repo / "docs/guide.md").write_text("[External](https://example.invalid/)\n")
        (self.repo / "app/deploy/linux").mkdir(parents=True)
        (self.repo / "app/app.pro").write_text(
            'VERSION = 1.2.3\nDEFINES += STARLIGHT_VERSION_STR=\\"$$VERSION-alpha.4\\"\n')
        (self.repo / "CHANGELOG.md").write_text("## 1.2.3-alpha.4 (test build)\n")
        self.meta = self.repo / "app/deploy/linux/com.starlight.Starlight.appdata.xml"
        self.meta.write_text('<component><releases><release version="1.2.3-alpha.4"/></releases></component>')

    def test_valid_source(self):
        self.assertEqual(repository.check(self.repo), [])

    def test_private_paths_are_rejected_at_any_depth(self):
        paths = [".claude/settings.local.json", "nested/.codex/session.jsonl",
                 "data/.ssh/config", ".gnupg/private-keys-v1.d/key", "host/.env",
                 "id_ed25519", "local/credentials.json"]
        self.assertEqual(len(repository.check_tracked_paths(paths)), len(paths))

    def test_public_examples_and_source_are_not_private_paths(self):
        self.assertEqual(repository.check_tracked_paths([
            ".env.example", "docs/codex.md", "tests/fixtures/certificate.pem",
            "app/supportinfo.cpp", "docs/credentials.json.md", ""]), [])

    def test_stale_metadata_and_changelog_are_rejected(self):
        self.meta.write_text('<component><releases><release version="1.2.3-alpha.3"/></releases></component>')
        (self.repo / "CHANGELOG.md").write_text("## 1.2.3-alpha.3\n")
        errors = repository.check(self.repo)
        self.assertTrue(any("AppStream" in e for e in errors))
        self.assertTrue(any("changelog" in e for e in errors))

    def test_broken_markdown_and_html_images_are_rejected(self):
        (self.repo / "docs/guide.md").write_text('[Missing](missing.md)\n<img src="missing.png">')
        self.assertEqual(len(repository.check(self.repo)), 2)

    def test_external_urls_and_fragments_need_no_network(self):
        (self.repo / "docs/guide.md").write_text(
            "[Section](#section-1) [Mail](mailto:test@example.invalid)\n## Section\n## Section\n")
        self.assertEqual(repository.check(self.repo), [])

    def test_links_to_missing_headings_are_rejected(self):
        (self.repo / "README.md").write_text('[In code](docs/guide.md#example)')
        (self.repo / "docs/guide.md").write_text('```sh\n# Example\n```\n')
        self.assertTrue(any("missing heading" in error for error in repository.check(self.repo)))

    def test_links_cannot_escape_repository(self):
        (self.repo / "README.md").write_text('[Private](../)')
        self.assertTrue(repository.check(self.repo))

    def test_flatpak_pin_rejects_changed_app_but_not_documentation(self):
        self.make_pinned_repo()
        self.assertEqual(repository.check_flatpak_pin(self.repo), [])
        (self.repo / "README.md").write_text("Updated documentation\n")
        self.assertEqual(repository.check_flatpak_pin(self.repo), [])
        with (self.repo / "app/app.pro").open("a") as out:
            out.write("DEFINES += CHANGED_BEHAVIOR\n")
        self.assertTrue(repository.check_flatpak_pin(self.repo))

    def make_pinned_repo(self):
        def git(*args):
            return subprocess.check_output(["git", "-C", str(self.repo), *args],
                                           text=True, stderr=subprocess.DEVNULL).strip()
        git("init", "-q")
        (self.repo / "LICENSE").write_text("synthetic notice\n")
        git("add", ".")
        git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
            "commit", "-qm", "candidate")
        pin = git("rev-parse", "HEAD")
        (self.repo / "flatpak").mkdir()
        (self.repo / "flatpak/com.starlight.Starlight.yaml").write_text(
            f'modules:\n  - name: starlight\n    sources:\n      - type: git\n        commit: {pin}\n')

    def test_pin_covers_installed_license_and_untracked_build_inputs(self):
        self.make_pinned_repo()
        (self.repo / "LICENSE").write_text("changed notice\n")
        self.assertTrue(repository.check_flatpak_pin(self.repo))
        (self.repo / "LICENSE").write_text("synthetic notice\n")
        (self.repo / "app/new-source.cpp").write_text("untracked input\n")
        self.assertTrue(repository.check_flatpak_pin(self.repo))

    def test_pin_rejects_malformed_or_ambiguous_sources_without_traceback(self):
        self.make_pinned_repo()
        manifest = self.repo / "flatpak/com.starlight.Starlight.yaml"
        for value in ('', '[]', 'modules: []', 'modules: [oops]', 'modules: [',
                      'modules: [{name: starlight, sources: [{type: dir, commit: "' + 'a'*40 + '"}]}]',
                      'modules: [{name: starlight, sources: []}, {name: starlight, sources: []}]'):
            with self.subTest(value=value):
                manifest.write_text(value)
                self.assertTrue(repository.check_flatpak_pin(self.repo))

    def test_commit_privacy_catches_details_removed_from_the_next_revision(self):
        def git(*args):
            return subprocess.check_output(["git", "-C", str(self.repo), *args], text=True).strip()

        git("init", "-q")
        git("config", "user.name", "Dependency Bot")
        git("config", "user.email", "1+dependency[bot]@users.noreply.github.com")
        private_path = "/home/" + "fixture-account/private-file"
        tracked = self.repo / "notes.md"
        tracked.write_text(private_path)
        git("add", "notes.md")
        git("commit", "-qm", f"Copied a note from {private_path}")
        intermediate = git("rev-parse", "HEAD")
        tracked.write_text("Example notes only.\n")
        git("add", "notes.md")
        git("commit", "-qm", "Replace earlier notes")

        scripts = self.repo / "scripts"
        scripts.mkdir()
        checker = scripts / "check-privacy.py"
        shutil.copy(ROOT / "scripts" / checker.name, checker)
        clean = subprocess.run([sys.executable, checker, "--commit", "HEAD"], capture_output=True)
        self.assertEqual(clean.returncode, 0, clean.stderr)
        failed = subprocess.run([sys.executable, checker, "--commit", intermediate], capture_output=True)
        self.assertEqual(failed.returncode, 1)
        self.assertEqual(failed.stderr.count(b"personal home path"), 2)
        self.assertNotIn(private_path.encode(), failed.stderr)
        self.assertNotIn(b"notes.md", failed.stderr)


class ScopeChecks(unittest.TestCase):
    def test_docs_skip_compilation(self):
        self.assertEqual(ci.scope(["README.md", "docs/assets/screen.png"]), (False, False))

    def test_qml_still_gets_tests_and_compiler(self):
        self.assertEqual(ci.scope(["app/gui/main.qml", "app/gui/settings.js"]), (True, False))

    def test_dependencies_and_resources_require_app_build(self):
        for path in ("moonlight-common-c/moonlight-common-c", "qmdnsengine/qmdnsengine",
                     "h264bitstream/h264_stream.c", "app/SDL_GameControllerDB",
                     "app/qml.qrc", "app/res/icon.svg", "app/languages/qml_fr.ts",
                     "unknown-input", ".github/workflows/build.yml"):
            with self.subTest(path=path):
                self.assertEqual(ci.scope([path]), (True, True))

    def test_release_always_builds(self):
        self.assertEqual(ci.scope(["README.md"], full=True), (True, True))
