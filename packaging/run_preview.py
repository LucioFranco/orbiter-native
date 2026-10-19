#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""First-run external asset setup and bounded launch for a local macOS preview.

Copy this script beside the preview's orbiter binary. NASSP/planetary artwork is
never shipped in the executable directory; the user supplies original inputs.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from uuid import uuid4

PREVIEW = Path(__file__).resolve().parent
MISSIONS = {
    "insertion": "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn",
    "liftoff": "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m.scn",
    "pdi": "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn",
}
NASSP_PIN = "e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9"
SOURCE_PREFIX = "orbiter/Addons/NASSP/"


class SetupError(Exception):
    pass


def run(*args):
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise SetupError(f"{' '.join(map(str, args[:2]))}: {result.stderr.strip()}")
    return result.stdout


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def manifest():
    result = json.loads((PREVIEW / "preview-manifest.json").read_text())
    if result.get("release_ready") or not result.get("local_validation_only"):
        raise SetupError("Unexpected preview release state")
    for name, expected in result["files_sha256"].items():
        path = PREVIEW / name
        if not path.is_file() or path.is_symlink() or sha256(path) != expected:
            raise SetupError(f"Preview file failed integrity check: {name}")
    allowed = set(result["files_sha256"]) | {"preview-manifest.json"}
    actual = {str(path.relative_to(PREVIEW)) for path in PREVIEW.rglob("*")
              if path.is_file() or path.is_symlink()}
    if actual != allowed:
        raise SetupError(f"Unexpected preview files: {sorted(actual ^ allowed)[:5]}")
    return result


def external_inputs(source, data, textures, package):
    source = source.resolve(strict=True)
    data = data.resolve(strict=True)
    textures = textures.resolve(strict=True)
    if run("git", "-C", str(source), "rev-parse", "HEAD").strip() != package["source"]["revision"]:
        raise SetupError("Engine source does not match the preview's source revision")
    if run("git", "-C", str(source), "status", "--porcelain", "--untracked-files=no").strip():
        raise SetupError("Engine source has tracked local changes; use the exact clean preview revision")
    if run("git", "-C", str(data), "rev-parse", "HEAD").strip() != NASSP_PIN:
        raise SetupError("NASSP data is not the pinned v9 release434 revision")
    if run("git", "-C", str(data), "status", "--porcelain", "--untracked-files=no").strip():
        raise SetupError("NASSP data has tracked local changes")
    ledger = PREVIEW / "v9-input-review.json"
    if sha256(ledger) != package["source"]["reviewed_input_ledger_sha256"]:
        raise SetupError("Preview source inventory is corrupted")
    bitmap_entries = [item for item in json.loads(ledger.read_text())["files"]
                      if "/Bitmaps/" in item["source"]]
    if len(bitmap_entries) != package["first_run_external_bitmaps"]:
        raise SetupError("Unexpected bitmap inventory length")
    bitmap_dir = data / "Orbitersdk/samples/ProjectApollo/Bitmaps"
    expected = set()
    for item in bitmap_entries:
        relative = item["source"].split(SOURCE_PREFIX, 1)[1]
        expected.add(Path(relative).relative_to("Orbitersdk/samples/ProjectApollo/Bitmaps"))
        path = data / relative
        if not path.is_file() or sha256(path) != item["sha256"]:
            raise SetupError(f"NASSP bitmap mismatch: {relative}")
    actual = {path.relative_to(bitmap_dir) for path in bitmap_dir.rglob("*")
              if path.is_file() and path.suffix.lower() == ".bmp"}
    if actual != expected:
        raise SetupError(f"NASSP bitmap set differs: missing={len(expected-actual)} extra={len(actual-expected)}")
    check = [sys.executable, str(source / "orbiter/tools/nassp_demo.py"), "check-data",
             "--profile", "v9", "--nassp-data-root", str(data), "--source-addon",
             str(source / "orbiter/Addons/NASSP"), "--verify-all-assets",
             "--orbiter-textures", str(textures)]
    run(*check)
    tool = source / "orbiter/tools/nassp_bitmap_resources.py"
    with tempfile.TemporaryDirectory(prefix="nassp-bitmap-preflight-") as temporary:
        temporary = Path(temporary)
        headers = []
        for label, root in (("adapted", source / SOURCE_PREFIX / "Orbitersdk/samples/ProjectApollo"),
                            ("original", data / "Orbitersdk/samples/ProjectApollo")):
            header = temporary / f"{label}.h"
            run(sys.executable, str(tool), "--source-root", str(root),
                "--asset-prefix", "Resources/NASSP", "--header", str(header),
                "--manifest", str(temporary / f"{label}.json"))
            headers.append(header.read_bytes())
        if headers[0] != headers[1]:
            raise SetupError("Original bitmap ID/path map differs from compiled source")
    return source, data, textures


def prepare(args, package):
    requested = args.runtime.expanduser().absolute()
    output = requested.parent.resolve(strict=True) / requested.name
    if output.exists() or output.is_symlink():
        raise SetupError("Choose a fresh nonexistent runtime directory")
    source, data, textures = external_inputs(args.source_root, args.nassp_data_root,
                                             args.orbiter_textures, package)
    for protected in (PREVIEW, source, data, textures):
        if output.is_relative_to(protected) or protected.is_relative_to(output):
            raise SetupError("Runtime must be outside package and external input trees")
    output.mkdir(parents=True)
    bitmap_source = data / "Orbitersdk/samples/ProjectApollo"
    run(sys.executable, str(source / "orbiter/tools/nassp_bitmap_resources.py"),
        "--source-root", str(bitmap_source), "--asset-prefix", "Resources/NASSP",
        "--output-bitmaps", str(output / "Resources/NASSP"),
        "--header", str(output / "bitmap-map.h"),
        "--manifest", str(output / "bitmap-map.json"))
    run(sys.executable, str(source / "orbiter/tools/stage_nassp_mission.py"), str(PREVIEW),
        "--profile", "v9", "--addon", str(data), "--textures", str(textures),
        "--scenario", MISSIONS[args.mission], "--output", str(output),
        "--resources-in-output")
    (output / "Orbiter.cfg").write_text("StartPaused = TRUE\nFixedStep = 0.016666666666666666\n")
    installed = {"schema": 1, "preview_root": str(PREVIEW), "source_root": str(source),
                 "nassp_data_root": str(data), "orbiter_textures": str(textures),
                 "mission": args.mission, "scenario": MISSIONS[args.mission],
                 "scenario_sha256": sha256(output / "Scenarios" / MISSIONS[args.mission]),
                 "bitmap_sha256": {str(path.relative_to(output / "Resources/NASSP")): sha256(path)
                                   for path in sorted((output / "Resources/NASSP").rglob("*.bmp"))},
                 "asset_policy": "original inputs are user acquired; only normalized bitmaps are copied locally"}
    installed["bitmap_count"] = len(installed["bitmap_sha256"])
    (output / "preview-install.json").write_text(json.dumps(installed, indent=2) + "\n")
    print(json.dumps({"runtime": str(output), "mission": args.mission,
                      "bitmap_count": installed["bitmap_count"],
                      "launch": f"python3 {PREVIEW / 'run-preview.py'} launch --runtime {output}"}, indent=2))


def launch(args, package):
    output = args.runtime.resolve(strict=True)
    installed = json.loads((output / "preview-install.json").read_text())
    if installed["preview_root"] != str(PREVIEW):
        raise SetupError("Package moved since setup; prepare a new runtime")
    if (output / "Modules").resolve(strict=True) != PREVIEW / "runtime/Modules":
        raise SetupError("Runtime modules no longer point to this preview")
    external_inputs(Path(installed["source_root"]), Path(installed["nassp_data_root"]),
                    Path(installed["orbiter_textures"]), package)
    scenario = output / "Scenarios" / installed["scenario"]
    if sha256(scenario) != installed["scenario_sha256"]:
        raise SetupError("Original scenario changed after setup")
    resource_root = output / "Resources/NASSP"
    actual_bitmaps = {str(path.relative_to(resource_root)) for path in resource_root.rglob("*.bmp")}
    if actual_bitmaps != set(installed["bitmap_sha256"]):
        raise SetupError("Runtime bitmap file set changed after setup")
    for name, expected in installed["bitmap_sha256"].items():
        file = resource_root / name
        if file.is_symlink() or sha256(file) != expected:
            raise SetupError(f"Runtime bitmap changed after setup: {name}")
    command = [str(PREVIEW / "orbiter"), "--root", str(output), "--scenario",
               installed["scenario"][:-4]]
    if args.headless:
        command += ["--headless", "--steps", "0", "--no-sound"]
    else:
        command += ["--graphics", "WgpuClient", "--module", "ProjectApolloMFD",
                    "--window", "--vc", "--laptop-controls"]
    save_name = ("Preview" + installed["mission"].capitalize() + "-" +
                 datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid4().hex[:6])
    command += ["--save", save_name]
    print(json.dumps({"command": command, "mode": "headless-load" if args.headless else "interactive"}))
    if not args.dry_run:
        completed = subprocess.run(command, cwd=output)
        if completed.returncode:
            raise SetupError(f"Simulator exited {completed.returncode}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    first = sub.add_parser("prepare", help="Verify external assets and create a fresh writable runtime")
    first.add_argument("--source-root", required=True, type=Path)
    first.add_argument("--nassp-data-root", required=True, type=Path)
    first.add_argument("--orbiter-textures", required=True, type=Path)
    first.add_argument("--runtime", required=True, type=Path)
    first.add_argument("--mission", choices=MISSIONS, default="liftoff")
    play = sub.add_parser("launch", help="Recheck inputs, then launch a prepared runtime")
    play.add_argument("--runtime", required=True, type=Path)
    play.add_argument("--headless", action="store_true", help="Load only, without GPU or audio")
    play.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    try:
        package = manifest()
        (prepare if args.action == "prepare" else launch)(args, package)
    except (OSError, SetupError, subprocess.SubprocessError, KeyError, ValueError) as error:
        parser.exit(2, f"Preview setup: {error}\n")


if __name__ == "__main__":
    main()
