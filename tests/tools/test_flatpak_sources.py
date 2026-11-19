import importlib.util
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("archive_flatpak_sources", SCRIPTS / "archive-flatpak-sources.py")
sources = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sources)


class SourceArchiveTest(unittest.TestCase):
    def test_mirror_portable_contents_are_independent_of_local_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            mirror = Path(temp) / "example.git"
            (mirror / "objects/pack").mkdir(parents=True)
            (mirror / "objects/pack/pack-object.pack").write_bytes(b"objects")
            (mirror / "HEAD").write_text("ref: refs/heads/main\n")
            (mirror / "config").write_text(f"[remote \"origin\"]\nurl = {temp}/cache\n")
            (mirror / "FETCH_HEAD").write_text(f"the cached file came from {temp}\n")
            (mirror / "hooks").mkdir()
            (mirror / "hooks/pre-push").write_bytes(b"local hook")
            out = io.BytesIO()
            with tarfile.open(fileobj=out, mode="w") as archive:
                sources.add_git_mirror(archive, mirror)
            with tarfile.open(fileobj=io.BytesIO(out.getvalue())) as archive:
                self.assertEqual(archive.extractfile("git/example.git/config").read(), sources.BARE_CONFIG)
                self.assertEqual(archive.extractfile("git/example.git/objects/pack/pack-object.pack").read(), b"objects")
                self.assertNotIn("git/example.git/FETCH_HEAD", archive.getnames())
                self.assertNotIn("git/example.git/hooks", archive.getnames())
                self.assertNotIn(temp.encode(), out.getvalue())
                self.assertTrue(all(m.uid == m.gid == m.mtime == 0 for m in archive))

    def test_mirror_with_external_object_store_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            mirror = Path(temp) / "example.git"
            (mirror / "objects/info").mkdir(parents=True)
            (mirror / "objects/info/alternates").write_text("/some/other/object/store\n")
            with tarfile.open(fileobj=io.BytesIO(), mode="w") as archive:
                with self.assertRaisesRegex(ValueError, "external object store"):
                    sources.add_git_mirror(archive, mirror)
