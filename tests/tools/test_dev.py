"""Guard the runner's isolation and failure reporting, not application behavior."""
import importlib.machinery
import importlib.util
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import signal
import sys
import tempfile
from types import SimpleNamespace
import unittest
import time
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2]
loader = importlib.machinery.SourceFileLoader("starlight_dev", str(SOURCE / "dev"))
spec = importlib.util.spec_from_loader(loader.name, loader)
dev = importlib.util.module_from_spec(spec)
loader.exec_module(dev)


class DevelopmentRunnerTests(unittest.TestCase):
    def test_changed_selection_keeps_shared_changes_conservative(self):
        available = dev.suites()
        with patch.object(subprocess, "check_output", side_effect=["docs/development.md\n", ""]):
            self.assertEqual(dev.changed_suites("main", available), set())
        with patch.object(subprocess, "check_output", side_effect=["app/gui/adaptive/gnome/GnomeAboutDialog.qml\n", ""]):
            selected = dev.changed_suites("main", available)
            self.assertIn("tst_gnomepages", selected)
            self.assertIn("tst_accessibility", selected)
            self.assertNotIn("tst_plasmapages", selected)
        with patch.object(subprocess, "check_output", side_effect=["", "new-build-input\n"]):
            self.assertEqual(dev.changed_suites("main", available), set(available))

    def test_managed_build_forwards_only_explicit_revision_override(self):
        for revision in (None, "identity fixture with spaces"):
            with self.subTest(revision=revision), tempfile.TemporaryDirectory() as directory:
                environment = {"UNRELATED_TEST_ENV": "do-not-forward"}
                if revision is not None:
                    environment["STARLIGHT_BUILD_REVISION"] = revision
                with patch.dict(os.environ, environment, clear=True), \
                        patch.object(dev.shutil, "which", return_value="/usr/bin/qmake6"), \
                        patch.object(subprocess, "check_output", return_value="6.12.0\n"), \
                        patch.object(subprocess, "run", return_value=SimpleNamespace(returncode=0)), \
                        patch.object(subprocess, "call", return_value=0) as launch:
                    self.assertEqual(dev.main(["build", "--build-dir", directory]), 0)
                command = launch.call_args.args[0]
                overrides = [value for value in command if value.startswith("--setenv=STARLIGHT_BUILD_REVISION=")]
                self.assertEqual(overrides, [] if revision is None else ["--setenv=STARLIGHT_BUILD_REVISION=" + revision])
                self.assertFalse(any("UNRELATED_TEST_ENV" in value for value in command))

    def test_private_environment_cannot_attach_to_user_session(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(os.environ, {"DISPLAY": ":0", "WAYLAND_DISPLAY": "wayland-0",
                                        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/user-bus",
                                        "STARLIGHT_TEST_GNOME_ADAPTER": "1"}):
                with dev.isolated_environment(directory) as env:
                    runtime = Path(env["XDG_RUNTIME_DIR"])
                    self.assertTrue(runtime.is_relative_to(directory))
                    self.assertEqual(runtime.stat().st_mode & 0o777, 0o700)
                    self.assertNotIn("DISPLAY", env)
                    self.assertNotIn("WAYLAND_DISPLAY", env)
                    self.assertNotIn("STARLIGHT_TEST_GNOME_ADAPTER", env)
                    self.assertNotEqual(env["DBUS_SESSION_BUS_ADDRESS"], "unix:path=/user-bus")
                self.assertFalse(runtime.exists())

    def test_timeout_is_a_failure_and_reaps_child_process(self):
        with tempfile.TemporaryDirectory() as directory:
            pidfile = Path(directory) / "child.pid"
            script = 'sleep 60 & printf "%s\\n" "$!" > "$1"; wait'
            code, _ = dev.execute(["sh", "-c", script, "timeout-fixture", str(pidfile)],
                                  Path(directory) / "run.log", os.environ.copy(), directory, 2)
            self.assertEqual(code, 124)
            self.assertTrue(pidfile.is_file(), "timeout fixture did not finish starting")
            pid = int(pidfile.read_text())
            # An orphan may briefly be a zombie until init reaps it; it must not run.
            stat = Path(f"/proc/{pid}/stat")
            self.assertTrue(not stat.exists() or stat.read_text().split()[2] == "Z")

    def test_success_exit_without_totals_is_not_a_passing_suite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "build/auto/example/tst_example"
            binary.parent.mkdir(parents=True)
            binary.write_text("#!/bin/sh\necho 'no tests executed'\n")
            binary.chmod(0o700)
            args = SimpleNamespace(function=[], style="Material", timeout=2)
            with contextlib.redirect_stdout(io.StringIO()):
                record = dev.run_suite("example", Path("example.pro"), args, root, root)
            self.assertFalse(record["ok"])
            binary.write_text("#!/bin/sh\necho 'Totals: 2 passed, 1 failed, 0 skipped'\n")
            with contextlib.redirect_stdout(io.StringIO()):
                record = dev.run_suite("example", Path("example.pro"), args, root, root)
            self.assertFalse(record["ok"])

    def test_unknown_selector_and_ambiguous_function_fail_before_build(self):
        for arguments in [["test", "does-not-exist"],
                          ["test", "smoke", "controller", "--function", "test_x"]]:
            with self.assertRaises(SystemExit) as result, contextlib.redirect_stderr(io.StringIO()):
                dev.main(arguments)
            self.assertEqual(result.exception.code, 2)

    def test_cache_from_another_checkout_is_refused_before_build(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "source.json").write_text(json.dumps(
                {"source": "/different/checkout", "qt": "6.4.2"}))
            with patch.object(dev.shutil, "which", return_value="/usr/bin/qmake6"), \
                    patch.object(subprocess, "check_output", return_value="6.4.2\n"), \
                    self.assertRaisesRegex(RuntimeError, "different source/Qt"):
                dev.main(["test", "smoke", "--no-scope", "--build-dir", directory])

    def test_interrupt_stops_executor_worker_without_waiting_for_test_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            pidfile = Path(directory) / "child.pid"
            child = (f"import pathlib,os,time; pathlib.Path({str(pidfile)!r})"
                     ".write_text(str(os.getpid())); time.sleep(60)")
            script = f'''
import concurrent.futures, os, pathlib, runpy, signal, sys, time
d = runpy.run_path({str(SOURCE / "dev")!r})
signal.signal(signal.SIGINT, d['interrupt'])
try:
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        pool.submit(d['execute'], [sys.executable, '-c', {child!r}],
                    pathlib.Path({str(Path(directory) / "worker.log")!r}),
                    os.environ.copy(), {directory!r}, 60)
        time.sleep(60)
except KeyboardInterrupt:
    sys.exit(130)
'''
            process = subprocess.Popen([sys.executable, "-c", script], start_new_session=True)
            try:
                deadline = time.monotonic() + 5
                while not pidfile.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(pidfile.exists())
                process.send_signal(signal.SIGINT)
                self.assertEqual(process.wait(timeout=3), 130)
                stat = Path(f"/proc/{pidfile.read_text()}/stat")
                self.assertTrue(not stat.exists() or stat.read_text().split()[2] == "Z")
            finally:
                dev.terminate(process)
                if pidfile.exists():
                    with contextlib.suppress(ProcessLookupError):
                        os.killpg(int(pidfile.read_text()), signal.SIGKILL)


if __name__ == "__main__":
    unittest.main()
