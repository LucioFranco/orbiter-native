#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Configure and compile the native Windows x64 host, NASSP modules and wgpu."""
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    if sys.platform != "win32" or platform.machine().upper() not in ("AMD64", "X86_64"):
        raise SystemExit("Native Windows CI requires a Windows x64 MSVC environment")
    cache = ROOT / ".ci-cache"
    build = ROOT / "orbiter/build/ci-windows"
    environment = dict(os.environ, CARGO_HOME=str(cache / "cargo"))
    command = ["cmake", "-S", str(ROOT / "orbiter"), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DORBITER_PORTABILITY_CHECKS=ON",
               "-DORBITER_BUILD_NASSP=ON", "-DORBITER_BUILD_WGPU=ON",
               "-DFETCHCONTENT_FULLY_DISCONNECTED=ON",
               "-DNASSP_DATA_DIR=" + str(cache / "nassp-v9"), "-DNASSP_DATA_PROFILE=v9",
               "-DORBITER_CARGO_HOME=" + str(cache / "cargo")]
    for name in json.loads((ROOT / "ci/inputs.json").read_text())["dependencies"]:
        command.append("-DFETCHCONTENT_SOURCE_DIR_" + name + "=" + str(cache / "deps" / name.lower()))
    subprocess.run(command, env=environment, check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", "2"],
                   env=environment, check=True)


if __name__ == "__main__":
    main()
