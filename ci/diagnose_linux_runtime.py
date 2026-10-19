#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Capture bounded Linux stacks for the full-world stall and sound crash."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def prepare_runtime(build, name, option):
    runtime = build / name
    if runtime.is_dir():
        return
    data = ROOT / ".ci-cache/nassp-v9"
    if not data.is_dir():
        print(f"## {name}: pinned data unavailable; diagnostic staging skipped", flush=True)
        return
    command = [sys.executable, str(ROOT / "orbiter/tools/stage_nassp_smoke.py"),
               str(build), "--addon", str(data), "--profile", "v9", option]
    try:
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL, timeout=30)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"## {name}: diagnostic staging failed: {error}", flush=True)


def child_of(process):
    children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
    try:
        ids = children.read_text().split()
        return int(ids[0]) if ids else None
    except (FileNotFoundError, ProcessLookupError):
        return None


def probe(label, command, interrupt):
    print(f"## {label}: {command[0].name}", flush=True)
    debugger = ["gdb", "--batch", "--quiet", "-ex", "set pagination off",
                "-ex", "set debuginfod enabled off", "-ex", "set startup-with-shell off"]
    if interrupt:
        debugger += ["-ex", "handle SIGUSR1 stop print nopass"]
    debugger += ["-ex", "run", "-ex", "thread apply all bt 25", "--args", *map(str, command)]
    with tempfile.TemporaryFile(mode="w+b") as output:
        process = subprocess.Popen(debugger, stdout=output, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        if interrupt:
            child = None
            deadline = time.monotonic() + 10
            while process.poll() is None and time.monotonic() < deadline:
                child = child_of(process)
                if child:
                    break
                time.sleep(0.1)
            if child and process.poll() is None:
                time.sleep(12)
                try:
                    os.kill(child, signal.SIGUSR1)
                except ProcessLookupError:
                    pass
        try:
            result = process.wait(timeout=25)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            result = "diagnostic timeout"
        output.seek(0, os.SEEK_END)
        size = output.tell()
        output.seek(max(0, size - 16000))
        print(f"gdb status={result}; captured_bytes={size}; final_bytes=16000", flush=True)
        print(output.read().decode("utf-8", "replace"), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    prepare_runtime(build, "runtime-pair", "--pair")
    prepare_runtime(build, "runtime-csm", "--csm")
    cases = [
        ("landing sound segfault", [build / "landing_sound_playback_test",
                                     build / "runtime-pair"], False),
        ("full-world stall", [build / "native_pause_test", build / "runtime-csm",
                               build / "core_module_fixture.so"], True),
    ]
    for label, command, interrupt in cases:
        if all(path.exists() for path in command):
            probe(label, command, interrupt)
        else:
            print(f"## {label}: build or staged runtime missing", flush=True)


if __name__ == "__main__":
    main()
