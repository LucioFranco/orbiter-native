#!/usr/bin/env python3
"""Export a deterministic, removable NASSP patch inventory from a verified checkout.

The reference must contain the specified exact commit (the pinned May baseline
by default). This does not modify either tree
or Git index. Source, configuration and scenarios are compared with CRLF
normalised; generated outputs and downloaded imagery are outside the patch scope.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

BASELINE = "96b6380943c382a297f969842be9344ef969a6f4"
SCOPES = ("Orbitersdk", "Config", "Scenarios")
GROUPS = {
    "build": ("Native source selection, linkage and platform adapters", "native_core, module_lifecycle, nassp_assets, native_original_insertion, native_original_liftoff", "Move build overlays to Orbiter; remove NASSP CMake edits when upstream supports the adapter"),
    "mission-integration": ("Mixed vessel input, audio, visuals and save-state integration; requires behavioral review", "native_nassp_resume, native_lm_resume, native_liftoff, native_liftoff_resume", "Move optional features behind platform services; retain only reviewed minimal hooks"),
    "input": ("Replace DirectInput with SDL/VESIM and release held controls", "native_v esim, nassp_joystick, nassp_joystick_flight, native_rcs".replace("v esim", "vesim"), "Use an upstream input adapter; remove native-specific vessel branches"),
    "audio": ("miniaudio/XRSound and temporary landing-callout implementation", "native_audio, native_sound, landing_sounds, landing_sound_playback", "Re-back the upstream audio interface; remove custom scheduler once upstream semantics are supported"),
    "graphics": ("Native surface, material, cockpit and CPU FDAI adapters", "native_fdai, fdai_rasterizer, wgpu_csm_cockpit, native_lm", "Move rendering implementation to Orbiter; remove direct backend dependencies from NASSP"),
    "guidance-portability": ("ROM/resource loading, fixed-width state and embedded process I/O; no guidance tuning", "nassp_assets, native_nassp_resume, native_lm_resume", "Adopt equivalent upstream fixes or a shared SDK/VFS interface; preserve ROM and state tests"),
    "compiler-portability": ("Compiler, path, initialisation and fixed-width compatibility; individually review non-mechanical hunks", "portable_sdk, panelsdk_math, nassp_assets, native_lm, native_rcs, native_original_insertion, native_original_liftoff", "Adopt upstream portability fixes; eliminate formatting-only churn and narrow remaining compatibility patches"),
}

def group(path):
    name = Path(path).name.lower()
    if name == "CMakeLists.txt".lower() or "/cmake/" in path: return "build"
    if name in ("lem.cpp", "saturn.cpp", "lem.h", "saturn.h"): return "mission-integration"
    if "vesim" in name or name in ("lemsystems.cpp", "satsystems.cpp", "lmscs.cpp"): return "input"
    if "sound" in name or name == "landingsounds.h": return "audio"
    if any(part in name for part in ("fdai", "panel", "mesh", "saturnvc", "lemvc", "mfd")) and "panelsdk/" not in path.lower(): return "graphics"
    if any(part in path.lower() for part in ("yaagc/", "yaags/")) or name in ("apolloguidance.cpp", "imu.cpp", "lm_ags.cpp", "dsky.cpp", "dsky.h"): return "guidance-portability"
    return "compiler-portability"

def read(path):
    return path.read_bytes().replace(b"\r\n", b"\n") if path.exists() else b""

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--addon", type=Path, default=Path(__file__).resolve().parents[1] / "Addons/NASSP")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--commit", default=BASELINE, help="Exact verified upstream commit")
    parser.add_argument("--repository", default="https://github.com/CaptainSwag101/NASSP.git")
    parser.add_argument("--ref", default="CMake")
    args = parser.parse_args()
    actual = subprocess.check_output(["git", "-C", str(args.reference), "rev-parse", "HEAD"], text=True).strip()
    if actual != args.commit: parser.error("reference HEAD does not match the specified exact baseline")
    if subprocess.check_output(["git", "-C", str(args.reference), "status", "--porcelain"], text=True).strip():
        parser.error("reference has changes; use a pristine checkout")
    paths = sorted({str(p.relative_to(tree)) for tree in (args.reference, args.addon)
                    for scope in SCOPES for p in (tree / scope).rglob("*") if p.is_file()})
    patches = {key: [] for key in GROUPS}
    entries = []
    identical = 0
    for path in paths:
        before, after = read(args.reference / path), read(args.addon / path)
        if before == after:
            identical += 1
            continue
        if b"\0" in before or b"\0" in after: raise RuntimeError("unexpected changed binary: " + path)
        key = group(path)
        exists_before, exists_after = (args.reference / path).exists(), (args.addon / path).exists()
        canonical_before = subprocess.check_output(["git", "-C", str(args.reference), "show", "HEAD:" + path]) if exists_before else b""
        canonical_after = after.replace(b"\n", b"\r\n") if b"\r\n" in canonical_before else after
        patch = ("diff --git a/{0} b/{0}\n".format(path)).encode()
        def blob_id(data):
            return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        old_id = blob_id(canonical_before) if exists_before else "0" * 40
        new_id = blob_id(canonical_after) if exists_after else "0" * 40
        patch += ("index " + old_id + ".." + new_id + "\n").encode()
        if not exists_before: patch += b"new file mode 100644\n"
        if not exists_after: patch += b"deleted file mode 100644\n"
        diff = difflib.diff_bytes(difflib.unified_diff, canonical_before.splitlines(True), canonical_after.splitlines(True),
                                 fromfile=("a/" + path).encode() if exists_before else b"/dev/null",
                                 tofile=("b/" + path).encode() if exists_after else b"/dev/null")
        patch += b"".join(line if line.endswith(b"\n") else line + b"\n\\ No newline at end of file\n" for line in diff)
        patches[key].append(patch)
        entries.append({"path": path, "group": key, "status": "modified" if exists_before and exists_after else "added" if exists_after else "deleted",
                        "baseline_sha256": hashlib.sha256(before).hexdigest() if exists_before else None,
                        "patched_sha256": hashlib.sha256(after).hexdigest() if exists_after else None,
                        "review": "native/reference behavioral parity review required"})
    args.output.mkdir(parents=True, exist_ok=True)
    ledger = {"schema": 1, "upstream": {"repository": args.repository, "ref": args.ref, "commit": args.commit},
              "scope": list(SCOPES), "normalisation": "CRLF to LF", "unchanged_files": identical,
              "groups": {}, "files": entries}
    for index, (key, (reason, tests, removal)) in enumerate(GROUPS.items(), 1):
        filename = f"{index:02d}-{key}.patch"
        (args.output / filename).write_bytes(b"".join(patches[key]))
        ledger["groups"][key] = {"patch": filename, "reason": reason, "test_gates": tests.split(", "), "removal_path": removal,
                                "removal_trigger": "equivalent upstream capability plus passing native/reference parity gates"}
    (args.output / "manifest.json").write_text(json.dumps(ledger, indent=2) + "\n")
    print(f"Verified {actual}: {identical} unchanged, {len(entries)} changed/added paths; exported {len(GROUPS)} removable patch groups")

if __name__ == "__main__": main()
