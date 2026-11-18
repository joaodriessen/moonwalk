#!/usr/bin/env python3
"""Bounded real-client reconnect trials. The caller supplies an isolated profile."""
import argparse
import ctypes
import codecs
import json
from datetime import datetime, timezone
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time


DONE = re.compile(r"SDL Info \(\d+\): done\s*$")
RENDER = re.compile(r"Rendering frame rate: ([0-9.]+) FPS")
METRICS = {
    "network_rtt": r"Average network latency: ([^\r\n]+)",
    "decode_ms": r"Average decoding time: ([0-9.]+) ms",
    "queue_ms": r"Average frame queue delay: ([0-9.]+) ms",
    "render_ms": r"Average rendering time \(including monitor V-sync latency\): ([0-9.]+) ms",
    "host_processing_ms_min_max_average": r"Host processing latency min/max/average: ([^\r\n]+)",
    "network_drop_percent": r"Frames dropped by your network connection: ([0-9.]+)%",
    "jitter_drop_percent": r"Frames (?:dropped due to network jitter|skipped to catch up with the display): ([0-9.]+)%",
}
MAX_LOG_BYTES = 32 * 1024 * 1024
CANCELLED = False


def stop(_signum, _frame):
    global CANCELLED
    CANCELLED = True


def parse_log(text):
    """Startup requires completed input startup; a video packet is not a render."""
    pending_input = False
    started = False
    for line in text.splitlines():
        if "Starting input stream..." in line:
            pending_input = True
        elif pending_input and DONE.search(line):
            started = True
            pending_input = False
        elif pending_input and ("Input stream start failed" in line or "Stopping input stream" in line):
            pending_input = False
    summary = text.rsplit("Global video stats", 1)[-1] if "Global video stats" in text else ""
    fps = RENDER.search(summary)
    result = {
        "input_start_completed": started,
        "first_video_packet": "Received first video packet after " in text,
        "rendered_fps": float(fps.group(1)) if fps else None,
        "global_video_summary": bool(summary),
    }
    result["rendered_frames_proven"] = bool(fps and float(fps.group(1)) > 0)
    result["metrics"] = {name: match.group(1) for name, pattern in METRICS.items()
                         if (match := re.search(pattern, summary))}
    return result


def identity(pid):
    try:
        fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        return fields[19], fields[0]  # start time, state; avoids PID reuse
    except (FileNotFoundError, ProcessLookupError):
        return None


def children(pid):
    found = set()
    for task in Path(f"/proc/{pid}/task").glob("*/children"):
        try:
            found.update(int(value) for value in task.read_text().split())
        except (FileNotFoundError, ProcessLookupError):
            pass
    return found


class OwnedProcesses:
    """Track only this trial and its descendants, including adopted orphans."""
    def __init__(self, command, log, env):
        self.baseline = children(os.getpid())
        self.identities = {}
        self.signalled = set()
        self.process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                        start_new_session=True, env=env)
        self.refresh()

    def refresh(self):
        pending = [self.process.pid] + list(children(os.getpid()) - self.baseline)
        seen = set()
        while pending:
            pid = pending.pop()
            if pid in seen:
                continue
            seen.add(pid)
            info = identity(pid)
            if info is None or (pid in self.identities and self.identities[pid] != info[0]):
                continue
            self.identities[pid] = info[0]
            pending.extend(children(pid))
        # A previously observed child may have detached into a new session.
        return [pid for pid, start in self.identities.items()
                if (info := identity(pid)) and info[0] == start and info[1] != "Z"]

    def send(self, pid, sig):
        key = (pid, self.identities.get(pid), sig)
        info = identity(pid)
        if key not in self.signalled and info and info[0] == key[1] and info[1] != "Z":
            try:
                os.kill(pid, sig)
                self.signalled.add(key)
            except ProcessLookupError:
                pass

    def client_signal(self, sig):
        alive = self.refresh()
        targets = []
        for pid in alive:
            try:
                if Path(os.readlink(f"/proc/{pid}/exe")).name == "starlight":
                    targets.append(pid)
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                pass
        # Flatpak's sandboxed child can have a different process group. Signal
        # that exact owned client once, rather than signalling wrapper + client.
        if not targets:
            if Path(self.process.args[0]).name == "flatpak":
                raise RuntimeError("No owned starlight executable found inside Flatpak")
            targets = [self.process.pid]
        for pid in targets:
            self.send(pid, sig)
        return targets

    def cleanup(self, deadline):
        # No pkill, flatpak kill, cgroups, services, or unrelated process groups.
        # SIGKILL is reserved for this trial's descendants after its exit budget.
        while True:
            alive = self.refresh()
            for pid in alive:
                self.send(pid, signal.SIGKILL)
            self.process.poll()
            for pid in self.identities:
                if pid == self.process.pid:
                    continue
                try:
                    os.waitpid(pid, os.WNOHANG)
                except ChildProcessError:
                    pass
            self.refresh()
            # A child may become a zombie just after waitpid(WNOHANG), or may
            # still be awaiting adoption from its exiting parent. Keep reaping
            # until the owned identities are gone, not merely no longer alive.
            remaining = [pid for pid, start in self.identities.items()
                         if (info := identity(pid)) and info[0] == start]
            if not remaining:
                return []
            if time.monotonic() >= deadline:
                return remaining
            time.sleep(0.02)


def copy_config(source, destination, deadline):
    for root, _directories, files in os.walk(source):
        if time.monotonic() >= deadline:
            raise TimeoutError("deadline while staging private config")
        target = destination / Path(root).relative_to(source)
        target.mkdir(parents=True, mode=0o700, exist_ok=True)
        for name in files:
            with (Path(root) / name).open("rb") as original, (target / name).open("xb") as copied:
                while chunk := original.read(64 * 1024):
                    if time.monotonic() >= deadline:
                        raise TimeoutError("deadline while staging private config")
                    copied.write(chunk)


def flatpak_prefix(prefix):
    """Accept flatpak run [--flag|--option=value] --command=CLIENT APPID.

    APPID must end the caller's prefix. Explicit --command (also accepted as
    two argv entries) lets the env bridge preserve the exact client executable.
    Other option values must use = to keep APPID unambiguous.
    """
    if Path(prefix[0]).name != "flatpak":
        return None
    if len(prefix) < 4 or prefix[1] != "run" or prefix[-1].startswith("-"):
        raise ValueError("Flatpak prefix must be flatpak run [OPTIONS] --command=CLIENT APPID")
    options = []
    executable = None
    index = 2
    while index < len(prefix) - 1:
        option = prefix[index]
        if option == "--command":
            index += 1
            if index >= len(prefix) - 1:
                raise ValueError("Flatpak --command requires a client executable before APPID")
            executable = prefix[index]
        elif option.startswith("--command="):
            executable = option.split("=", 1)[1]
        elif option.startswith("--") and option != "--":
            options.append(option)
        else:
            raise ValueError("Flatpak APPID must end the client prefix; option values must use --option=value")
        index += 1
    if not executable or executable.startswith("-") or "=" in executable:
        raise ValueError("Flatpak prefix requires --command=CLIENT with a nonempty executable without =")
    return {"length": len(prefix), "options": options, "app": prefix[-1], "executable": executable}


def sandbox_command(command, prefix, state, env):
    # Flatpak resets reserved XDG_* variables after --env processing. Execute
    # env *inside* the sandbox, while retaining caller XDG paths outside so
    # Flatpak can find its user installation. No shell parses paths or argv.
    assignments = [key + "=" + env[key] for key in
                   ("XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME")]
    return ([command[0], "run", "--filesystem=" + str(state.resolve()), *prefix["options"],
             "--command=env", prefix["app"], "--", *assignments, prefix["executable"]]
            + command[prefix["length"]:])


def run_trial(command, output, label, abrupt, args, total_deadline):
    start = time.monotonic()
    hard_deadline = min(start + args.startup_budget + args.seconds + args.shutdown_budget,
                        total_deadline - 2)
    result = {"trial": label, "mode": "abrupt" if abrupt else "graceful",
              "command": command, "passed": False, "log": str(output), "failure": None,
              "exit_expectation": "after requested SIGKILL" if abrupt else "zero after graceful SIGTERM"}
    owner = None
    text = ""
    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    stream_at = None
    signal_at = None
    try:
        state = output.parent / (label + "-state")
        state.mkdir(mode=0o700)
        copy_config(args.config_home, state / "config", hard_deadline)
        launcher_env = os.environ.copy()
        env = launcher_env.copy()
        for kind in ("CONFIG", "DATA", "CACHE", "STATE"):
            folder = state / kind.lower()
            folder.mkdir(mode=0o700, exist_ok=True)
            env["XDG_" + kind + "_HOME"] = str(folder.resolve())
        trial_command = list(command)
        if args.flatpak_prefix:
            trial_command = sandbox_command(command, args.flatpak_prefix, state, env)
            env = launcher_env
        result["command"] = trial_command
        if time.monotonic() >= hard_deadline:
            raise TimeoutError("deadline before client launch")
        with output.open("xb", buffering=0) as log, output.open("rb") as reader:
            owner = OwnedProcesses(trial_command, log, env)
            while True:
                now = time.monotonic()
                if CANCELLED:
                    result["failure"] = "harness interrupted"
                    break
                owner.refresh()
                chunk = reader.read(256 * 1024)
                text += decoder.decode(chunk)
                if output.stat().st_size > MAX_LOG_BYTES:
                    result["failure"] = "log size exceeded 32 MiB"
                    break
                evidence = parse_log(text)
                if signal_at is None and "Connection terminated:" in text:
                    result["failure"] = "stream terminated before requested disconnect"
                    break
                code = owner.process.poll()
                if code is not None:
                    # Drain bytes emitted during normal decoder destruction.
                    text += decoder.decode(reader.read(MAX_LOG_BYTES), final=True)
                    result["exit_code"] = code
                    if signal_at is None:
                        result["failure"] = "client exited before requested disconnect"
                    elif not abrupt and code != 0:
                        result["failure"] = "unexpected nonzero graceful exit"
                    break
                if now >= hard_deadline:
                    result["failure"] = "trial/total deadline exceeded"
                    break
                if stream_at is None:
                    if evidence["input_start_completed"] and evidence["first_video_packet"]:
                        stream_at = now
                        result["startup_seconds"] = round(now - start, 3)
                    elif now - start >= args.startup_budget:
                        result["failure"] = "startup budget exceeded without completed input and video packet"
                        break
                elif signal_at is None and now - stream_at >= args.seconds:
                    result["signal_targets"] = owner.client_signal(signal.SIGKILL if abrupt else signal.SIGTERM)
                    signal_at = now
                    result["observed_stream_seconds"] = round(now - stream_at, 3)
                elif signal_at is not None and now - signal_at >= args.shutdown_budget:
                    result["failure"] = "disconnect exit budget exceeded"
                    break
                time.sleep(0.05)
    except (OSError, RuntimeError, KeyboardInterrupt) as error:
        result["failure"] = f"{type(error).__name__}: {error}"
    finally:
        if owner:
            result["forced_cleanup_pids"] = owner.refresh()
            if result["forced_cleanup_pids"] and not abrupt and not result["failure"]:
                result["failure"] = "owned processes remained after graceful client exit"
            leftovers = owner.cleanup(min(total_deadline, time.monotonic() + 2))
            result["owned_processes_remaining"] = leftovers
            if leftovers:
                result["failure"] = "owned processes survived cleanup deadline"
        result["seconds"] = round(time.monotonic() - start, 3)
        result["evidence"] = parse_log(text)
        if not result["failure"]:
            if not result["evidence"]["input_start_completed"] or not result["evidence"]["first_video_packet"]:
                result["failure"] = "missing connection/first-packet evidence"
            elif not abrupt and not result["evidence"]["rendered_frames_proven"]:
                result["failure"] = "missing positive rendered-frame global summary"
        result["passed"] = result["failure"] is None
        if abrupt:
            result["render_limit"] = "SIGKILL cannot flush decoder summary; only following graceful reconnect proves rendering"
    return result


def positive(value):
    number = float(value)
    if not 0 < number <= 600:
        raise argparse.ArgumentTypeError("must be greater than zero and at most 600 seconds")
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--app", default="Desktop")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--config-home", type=Path, required=True, help="existing private copied XDG config; never changed")
    parser.add_argument("--stream-arg", action="append", default=[], help="additional stream argument; use --stream-arg=--1080")
    parser.add_argument("--cycles", type=int, default=2, help="graceful trials, 2..10")
    parser.add_argument("--seconds", type=positive, default=15, help="hold time after startup")
    parser.add_argument("--startup-budget", type=positive, default=60)
    parser.add_argument("--shutdown-budget", type=positive, default=15)
    parser.add_argument("--total-budget", type=positive, default=600)
    parser.add_argument("--abrupt", action="store_true", help="also kill one client and gracefully reconnect")
    parser.add_argument("client", nargs=argparse.REMAINDER, help="-- CLIENT [client prefix arguments]")
    args = parser.parse_args(argv)
    command = args.client[1:] if args.client[:1] == ["--"] else args.client
    if not command or not 2 <= args.cycles <= 10:
        parser.error("provide -- CLIENT [arguments] and --cycles between 2 and 10")
    try:
        args.flatpak_prefix = flatpak_prefix(command)
    except ValueError as error:
        parser.error(str(error))
    args.config_home = args.config_home.resolve()
    production = [Path.home() / ".config", Path.home() / ".var/app"]
    if (not args.config_home.is_dir() or
        any(args.config_home == path or path in args.config_home.parents for path in production) or
        any(path.is_symlink() for path in args.config_home.rglob("*"))):
        parser.error("--config-home must be a private copied directory outside production config, without symlinks")
    # Linux subreaper retains ownership of sandbox/double-fork descendants, even
    # after their original parent exits. It affects this process only.
    if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), "cannot establish child subreaper")
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    args.output.mkdir(parents=True, mode=0o700, exist_ok=False)
    command += ["stream", args.host, args.app] + args.stream_arg + ["--no-quit-after"]
    deadline = time.monotonic() + args.total_budget
    report = {"schema": 1, "started_utc": datetime.now(timezone.utc).isoformat(),
              "total_budget_seconds": args.total_budget, "trials": [], "manual_events": {
        name: "not executed or certified by this harness"
        for name in ("sleep_wake", "audio_device_change", "display_change", "clipboard_focus", "physical_input")}}
    schedule = [(f"graceful-{index + 1}", False) for index in range(args.cycles)]
    if args.abrupt:
        schedule += [("abrupt", True), ("after-abrupt-reconnect", False)]
    for label, abrupt in schedule:
        if CANCELLED or time.monotonic() >= deadline - 2:
            report["failure"] = "harness interrupted" if CANCELLED else "total budget exhausted before next trial"
            break
        trial = run_trial(command, args.output / f"{label}.log", label, abrupt, args, deadline)
        report["trials"].append(trial)
        print(f"{'PASS' if trial['passed'] else 'FAIL'} {label}: {trial['failure'] or 'observed requested lifecycle'}", flush=True)
        if not trial["passed"]:
            break
    report["passed"] = len(report["trials"]) == len(schedule) and all(t["passed"] for t in report["trials"])
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    (args.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
