#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the initial native CI probes; failures propagate without waivers."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nassp", action="store_true")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.nassp and (sys.platform != "darwin" or platform.machine() != "arm64"):
        parser.error("Unsupported v9 CI target: full NASSP build is scoped to macOS arm64")
    if not args.nassp and (sys.platform != "linux" or platform.machine() not in ("x86_64", "AMD64")):
        parser.error("Unsupported engine CI target: this foundation probes Linux x64 only")
    cache, build = ROOT / ".ci-cache", ROOT / "orbiter/build/ci"
    environment = dict(os.environ, CARGO_HOME=str(cache / "cargo"))
    command = ["cmake", "-S", str(ROOT / "orbiter"), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DORBITER_PORTABILITY_CHECKS=ON",
               "-DFETCHCONTENT_FULLY_DISCONNECTED=ON",
               "-DORBITER_BUILD_NASSP=" + ("ON" if args.nassp else "OFF"),
               "-DORBITER_BUILD_WGPU=" + ("ON" if args.nassp else "OFF"),
               "-DORBITER_CARGO_HOME=" + str(cache / "cargo")]
    for name in json.loads((ROOT / "ci/inputs.json").read_text())["dependencies"]:
        command.append("-DFETCHCONTENT_SOURCE_DIR_" + name + "=" + str(cache / "deps" / name.lower()))
    if args.nassp:
        command += ["-DNASSP_DATA_DIR=" + str(cache / "nassp-v9"),
                    "-DNASSP_DATA_PROFILE=v9", "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0"]
    else:
        command += ["-DSDL_X11=OFF", "-DSDL_WAYLAND=OFF", "-DSDL_UNIX_CONSOLE_BUILD=ON"]
    subprocess.run(command, env=environment, check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)],
                   env=environment, check=True)
    subprocess.run([sys.executable, str(ROOT / "ci/test_plan.py"), str(build)],
                   env=environment, check=True)


if __name__ == "__main__":
    main()
