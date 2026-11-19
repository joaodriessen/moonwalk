"""Protocol-log and owned-process fixtures; never opens a stream or real profile."""
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/validation/stream-recovery.py"
SPEC = importlib.util.spec_from_file_location("stream_recovery", SCRIPT)
recovery = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(recovery)
START = "00:00:03 - SDL Info (0): Starting input stream...\n00:00:03 - SDL Info (0): done\n"
PACKET = "00:00:03 - SDL Info (0): Received first video packet after 0 ms\n"
SUMMARY = (
    "Global video stats\n------------------\nIncoming frame rate from network: 60.0 FPS\n"
    "Rendering frame rate: 59.77 FPS\nAverage network latency: 9 ms (variance: 4 ms)\n"
)


class RecoveryTests(unittest.TestCase):
    def test_packet_or_failed_input_is_not_render_proof(self):
        self.assertFalse(recovery.parse_log(PACKET)["input_start_completed"])
        self.assertFalse(recovery.parse_log(START + PACKET)["rendered_frames_proven"])
        failed = START.split("00:00:03 - SDL Info (0): done")[0] + "Input stream start failed: 1\n"
        self.assertFalse(recovery.parse_log(failed)["input_start_completed"])
        result = recovery.parse_log(START + PACKET + SUMMARY)
        self.assertTrue(result["rendered_frames_proven"])
        self.assertEqual(result["metrics"]["network_rtt"], "9 ms (variance: 4 ms)")
        self.assertEqual(result["metrics"]["incoming_fps"], "60.0")
        self.assertFalse(recovery.parse_log(SUMMARY.replace("59.77", "0.00"))["rendered_frames_proven"])

    def test_frame_gaps_use_their_own_metric(self):
        summary = SUMMARY.replace("Incoming frame rate from network", "Frame rate submitted for decoding")
        metrics = recovery.parse_log(summary + "Frames missing before decoding: 42.5%\n")["metrics"]
        self.assertEqual(metrics["incoming_fps"], "60.0")
        self.assertEqual(metrics["before_decode_gap_percent"], "42.5")
        self.assertNotIn("network_drop_percent", metrics)

    def run_fixture(self, root, code, extra=()):
        config = root / "copied-config"
        config.mkdir()
        (config / "fixture.conf").write_text("preserve me")
        fixture = root / "client.py"
        fixture.write_text(code)
        output = root / "results"
        command = [sys.executable, str(SCRIPT), "--host", "fixture-host", "--config-home", str(config),
                   "--output", str(output), "--seconds", "0.1", "--startup-budget", "0.4",
                   "--shutdown-budget", "0.4", "--total-budget", "8", *extra,
                   "--", sys.executable, str(fixture)]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=12)
        self.assertEqual((config / "fixture.conf").read_text(), "preserve me")
        return completed, json.loads((output / "summary.json").read_text())

    def test_flatpak_launcher_keeps_installation_paths_and_isolates_sandbox(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config = root / "copied-config"
            config.mkdir()
            capture = root / "launcher.json"
            env = os.environ.copy()
            original = {}
            for kind in ("CONFIG", "DATA", "CACHE", "STATE"):
                folder = root / ("outer-" + kind.lower())
                folder.mkdir()
                key = "XDG_" + kind + "_HOME"
                original[key] = env[key] = str(folder)
            (Path(env["XDG_DATA_HOME"]) / "installed-app").touch()
            env["FIXTURE_CAPTURE"] = str(capture)
            launcher = root / "flatpak"
            inner = root / "inner.json"
            client = root / "client with spaces.py"
            client.write_text("#!" + sys.executable + "\n" +
                "import json,os,pathlib,sys\n" +
                "pathlib.Path(os.environ['FIXTURE_INNER']).write_text(json.dumps({'argv':sys.argv,'env':{k:v for k,v in os.environ.items() if k.startswith('XDG_')}}))\n" +
                "pathlib.Path(os.environ['XDG_CONFIG_HOME'],'trial-only').touch()\n")
            client.chmod(0o755)
            env['FIXTURE_INNER'] = str(inner)
            launcher.write_text("#!" + sys.executable + "\n" +
                "import json,os,pathlib,sys,subprocess\n" +
                "if not pathlib.Path(os.environ['XDG_DATA_HOME'],'installed-app').is_file(): sys.exit(23)\n" +
                "pathlib.Path(os.environ['FIXTURE_CAPTURE']).write_text(json.dumps({'argv':sys.argv,'env':{k:v for k,v in os.environ.items() if k.startswith('XDG_')}}))\n" +
                "if '--command=env' not in sys.argv: sys.exit(24)\n" +
                "inner_env=os.environ.copy()\n" +
                "for key in ('XDG_CONFIG_HOME','XDG_DATA_HOME','XDG_CACHE_HOME','XDG_STATE_HOME'): inner_env[key]='/reset-by-flatpak'\n" +
                "sys.exit(subprocess.run(['/usr/bin/env',*sys.argv[sys.argv.index('fixture.app')+1:]],env=inner_env).returncode)\n")
            launcher.chmod(0o755)
            output = root / "results"
            completed = subprocess.run([sys.executable, str(SCRIPT), "--host", "fixture-host",
                "--config-home", str(config), "--output", str(output), "--seconds", "0.1",
                "--startup-budget", "0.4", "--total-budget", "8", "--", str(launcher),
                "run", "--socket=x11", "--command=" + str(client), "fixture.app"], env=env, capture_output=True,
                text=True, timeout=10)
            # The launcher fixture deliberately exits without starting a stream.
            self.assertEqual(completed.returncode, 1)
            self.assertTrue(capture.exists(), completed.stdout + completed.stderr)
            observed = json.loads(capture.read_text())
            self.assertTrue(inner.exists(), completed.stdout + completed.stderr)
            sandbox = json.loads(inner.read_text())
            for key, value in original.items():
                self.assertEqual(observed["env"][key], value)
                self.assertNotEqual(sandbox["env"][key], value)
                self.assertTrue(Path(sandbox["env"][key]).is_relative_to(output))
            self.assertIn("--filesystem=" + str(output / "graceful-1-state"), observed["argv"])
            self.assertIn("--socket=x11", observed["argv"])
            self.assertEqual(sandbox["argv"][0], str(client))
            self.assertEqual(sandbox["argv"][1:4], ["stream", "fixture-host", "Desktop"])
            self.assertTrue((Path(sandbox["env"]["XDG_CONFIG_HOME"]) / "trial-only").exists())
            self.assertFalse((config / "trial-only").exists())

    def test_flatpak_prefix_requires_unambiguous_app_and_keeps_explicit_client(self):
        prefix = ["flatpak", "run", "--socket=x11", "--command", "/app/bin/custom client", "fixture.app//branch"]
        parsed = recovery.flatpak_prefix(prefix)
        self.assertEqual(parsed["executable"], "/app/bin/custom client")
        self.assertEqual(parsed["app"], "fixture.app//branch")
        self.assertEqual(parsed["options"], ["--socket=x11"])
        self.assertIsNone(recovery.flatpak_prefix(["client", "prefix-argument"]))
        for invalid in (["flatpak", "run", "fixture.app"],
                        ["flatpak", "run", "--command=starlight", "fixture.app", "extra"],
                        ["flatpak", "run", "--arch", "x86_64", "--command=starlight", "fixture.app"],
                        ["flatpak", "run", "--command=", "fixture.app"]):
            with self.subTest(prefix=invalid), self.assertRaises(ValueError):
                recovery.flatpak_prefix(invalid)

    def test_graceful_abrupt_and_reconnect(self):
        code = "import signal,sys,time,os,pathlib\n"
        code += "pathlib.Path(os.environ['XDG_CONFIG_HOME'], 'fixture.conf').write_text('trial only')\n"
        code += "def stop(*args):\n print(" + repr(SUMMARY) + ", flush=True)\n sys.exit(0)\n"
        code += "signal.signal(signal.SIGTERM, stop)\nprint(" + repr(START + PACKET) + ", flush=True)\ntime.sleep(60)\n"
        with tempfile.TemporaryDirectory() as temporary:
            completed, report = self.run_fixture(Path(temporary), code, ["--abrupt", "--after-abrupt-delay", "0.2"])
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertTrue(report["passed"])
        self.assertEqual(len(report["trials"]), 4)
        self.assertEqual(report["after_abrupt_delay_seconds"], 0.2)
        self.assertFalse(report["trials"][2]["evidence"]["rendered_frames_proven"])
        self.assertTrue(report["trials"][3]["evidence"]["rendered_frames_proven"])
        self.assertIn("not executed", report["manual_events"]["sleep_wake"])
        self.assertTrue(all(not trial["owned_processes_remaining"] for trial in report["trials"]))

    def test_timeout_reaps_detached_child_and_preserves_unrelated_process(self):
        sentinel = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(60)"])
        def cleanup_sentinel():
            if sentinel.poll() is None:
                sentinel.kill()
            sentinel.wait(timeout=2)
        self.addCleanup(cleanup_sentinel)
        code = ("import subprocess,sys,time\n"
                "child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)'],start_new_session=True)\n"
                "print('DETACHED_CHILD='+str(child.pid),flush=True)\ntime.sleep(60)\n")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            before = time.monotonic()
            completed, report = self.run_fixture(root, code)
            log = (root / "results/graceful-1.log").read_text()
            child_pid = int(log.split("DETACHED_CHILD=", 1)[1].splitlines()[0])
        self.assertLess(time.monotonic() - before, 4)
        self.assertEqual(completed.returncode, 1, completed.stdout + completed.stderr)
        self.assertIn("startup budget", report["trials"][0]["failure"])
        self.assertIsNone(recovery.identity(child_pid), "detached owned child survived")
        self.assertIsNone(sentinel.poll(), "unrelated process was signalled")
        sentinel.kill()
        sentinel.wait()

    def test_cleanup_waits_for_owned_zombie_after_nonblocking_reap_race(self):
        # Model the real hosted failure: WNOHANG sees a not-yet-waitable child;
        # it exits before the next /proc refresh, so no running child remains.
        owned = recovery.OwnedProcesses.__new__(recovery.OwnedProcesses)
        owned.process = mock.Mock(pid=10)
        owned.identities = {11: "captured-start"}
        owned.refresh = mock.Mock(return_value=[])
        reaped = False
        attempts = 0
        def waitpid(pid, options):
            nonlocal reaped, attempts
            self.assertEqual(pid, 11)
            attempts += 1
            reaped = attempts > 1
            return (11, 0) if reaped else (0, 0)
        def identity(pid):
            return None if reaped else ("captured-start", "Z")
        with mock.patch.object(recovery, "identity", side_effect=identity), \
                mock.patch.object(recovery.os, "waitpid", side_effect=waitpid), \
                mock.patch.object(recovery.time, "sleep"):
            self.assertEqual(owned.cleanup(time.monotonic() + 1), [])
        self.assertTrue(reaped, "cleanup returned before reaping its owned child")

    def test_missing_summary_and_nonzero_exit_fail(self):
        for body in ("sys.exit(0)", "print(" + repr(SUMMARY) + ",flush=True);sys.exit(3)"):
            code = "import signal,sys,time\ndef stop(*args):\n " + body + "\n"
            code += "signal.signal(signal.SIGTERM,stop)\nprint(" + repr(START + PACKET) + ",flush=True)\ntime.sleep(60)\n"
            with tempfile.TemporaryDirectory() as temporary:
                completed, report = self.run_fixture(Path(temporary), code)
            self.assertEqual(completed.returncode, 1)
            self.assertFalse(report["passed"])


if __name__ == "__main__":
    unittest.main()
