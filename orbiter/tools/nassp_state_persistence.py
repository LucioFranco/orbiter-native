#!/usr/bin/env python3
"""Append a removable persistence overlay to an existing verified NASSP stack.

Reconstruct the existing seven adapter groups without 08-state-persistence.patch
to supply --previous-tree. This preserves all earlier patch bytes and isolates
upstream loader fixes from native adaptations in the same source file.
"""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

FILES = ["Orbitersdk/samples/ProjectApollo/src_lm/" + name for name in
         ("lm_lr.cpp", "lemsystems.cpp", "lm_dps.cpp", "LEM.cpp", "LEM.h")]


def sha(data):
    return hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest()


def blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("reference", "previous-tree", "addon", "stack"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    manifest_path = args.stack / "manifest.json"
    ledger = json.loads(manifest_path.read_text())
    head = subprocess.check_output(["git", "-C", str(args.reference), "rev-parse", "HEAD"], text=True).strip()
    assert head == ledger["upstream"]["commit"]
    assert not subprocess.check_output(["git", "-C", str(args.reference), "status", "--porcelain"])
    entries = {entry["path"]: entry for entry in ledger["files"]}
    patches = []
    for path in FILES:
        upstream = subprocess.check_output(["git", "-C", str(args.reference), "show", "HEAD:" + path])
        before = (args.previous_tree / path).read_bytes()
        after = (args.addon / path).read_bytes()
        prior = entries.get(path)
        expected = prior.get("overlay_base_sha256", prior["patched_sha256"]) if prior else sha(upstream)
        assert sha(before) == expected, "previous adapter mismatch: " + path
        before = before.replace(b"\r\n", b"\n")
        after = after.replace(b"\r\n", b"\n")
        if b"\r\n" in upstream:
            before = before.replace(b"\n", b"\r\n")
            after = after.replace(b"\n", b"\r\n")
        assert before != after, "expected a persistence change: " + path
        patch = (f"diff --git a/{path} b/{path}\nindex {blob(before)}..{blob(after)}\n").encode()
        diff = difflib.diff_bytes(difflib.unified_diff, before.splitlines(True), after.splitlines(True),
                                  fromfile=("a/" + path).encode(), tofile=("b/" + path).encode())
        patch += b"".join(line if line.endswith(b"\n") else line + b"\n\\ No newline at end of file\n" for line in diff)
        patches.append(patch)
        if prior is None:
            prior = {"path": path, "group": "state-persistence", "status": "modified",
                     "baseline_sha256": sha(upstream), "review": "Upstream loader correction; Windows behavior not independently executed"}
            entries[path] = prior
            ledger["unchanged_files"] -= 1
        elif prior["group"] != "state-persistence":
            prior["additional_groups"] = sorted(set(prior.get("additional_groups", [])) | {"state-persistence"})
        prior["overlay_base_sha256"] = sha(before)
        prior["patched_sha256"] = sha(after)
    ledger["groups"]["state-persistence"] = {
        "patch": "08-state-persistence.patch",
        "reason": "Temporary upstream exact-token/complete-value loader fixes for LR range, right pointer and gimbal motor flags, plus persistence of the existing DPS throttle command and restoration of existing cached CoG/PMI/last-update fuel weight before the first frame; no equation changes",
        "test_gates": ["native_lm_persistence", "native_pdi_pilot", "native_lm_resume", "native_original_descent"],
        "removal_path": "Adopt upstream fixes for the three loaders and throttle persistence; adopt cached mass/CoG persistence and restoration before the first frame, then remove this overlay, regenerate the adapter inventory against the new pin and rerun persistence/neutral continuation gates",
        "removal_trigger": "equivalent upstream loader/state fixes and pre-step initialization plus exact zero-step and bounded continuation checks in both supported revisions",
        "port_specific": False,
        "state_restore_lifecycle": "LEM.cpp/LEM.h persist cached CoG/PMI/fuel history and reuse original setup in post-creation; independent Windows execution deferred",
    }
    ledger["files"] = sorted(entries.values(), key=lambda entry: entry["path"])
    (args.stack / "08-state-persistence.patch").write_bytes(b"".join(patches))
    manifest_path.write_text(json.dumps(ledger, indent=2) + "\n")
    print("Exported five files with loader/throttle persistence corrections; existing seven patch groups preserved")


if __name__ == "__main__":
    main()
