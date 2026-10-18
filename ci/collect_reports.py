#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Collect bounded reports into a fresh directory; authorize upload only on success."""
import argparse
import json
import os
from pathlib import Path
import stat
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 5 * 1024 * 1024
REPORTS = ("test-plan.json", "test-result.json", "ctest.xml")


def step_output(name, value):
    if "\n" in value or "\r" in value:
        raise ValueError("Invalid workflow output value")
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as stream:
            stream.write(name + "=" + value + "\n")


def main():
    # A failed collector never authorizes stale files left by another process/run.
    step_output("ready", "false")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--scope", required=True)
    args = parser.parse_args()
    base = ROOT / "ci-reports"
    if base.is_symlink() or (base.exists() and not base.is_dir()):
        raise ValueError("Report output base must be a regular directory")
    if base.exists() and any(p.is_symlink() for p in base.rglob("*")):
        raise ValueError("Report output base must not contain symlinks")
    payload = {}
    for name in REPORTS:
        path = args.build / name
        if not path.exists() and not path.is_symlink():
            continue
        if path.is_symlink() or not path.is_file():
            raise ValueError("Report publication refused: unsafe report path")
        # Snapshot bounded bytes once; later copying cannot follow a changed link
        # or include bytes appended after the size check.
        with path.open("rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise ValueError("Report input must be a regular file")
            payload[name] = stream.read(LIMIT + 1)
        if sum(map(len, payload.values())) > LIMIT:
            raise ValueError("Report publication refused: 5 MiB cap exceeded")
    context = json.loads((ROOT / "ci/acceptance-context.json").read_text())
    summary = {"scope": args.scope, "full_acceptance": False,
               "binary_preview_uploaded": False, "historical_acceptance": context,
               "current_test_status": "incomplete"}
    if "ctest.xml" in payload:
        xml = ET.fromstring(payload["ctest.xml"])
        summary["junit_counts"] = {k: int(xml.attrib.get(k, 0)) for k in ("tests", "failures", "errors", "skipped")}
        if summary["junit_counts"]["tests"]:
            summary["current_test_status"] = "failed" if any(summary["junit_counts"][k] for k in ("failures", "errors")) else "passed subset"
    summary_text = json.dumps(summary, indent=2) + "\n"
    payload["scope.json"] = summary_text.encode()
    if sum(map(len, payload.values())) > LIMIT:
        raise ValueError("Report publication refused: 5 MiB cap exceeded")
    base.mkdir(exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="validated-", dir=base))
    for name, data in payload.items():
        (output / name).write_bytes(data)
    if set(p.name for p in output.iterdir()) != set(payload):
        raise ValueError("Unexpected file in validated report output")
    for name, data in payload.items():
        path = output / name
        if path.is_symlink() or not path.is_file() or path.read_bytes() != data:
            raise ValueError("Validated report output changed")
    text = "## Capability-limited CI result\n\n" + summary_text + "\nFull acceptance remains 103/104 in the dated source record; native_pdi_pilot is a strict known failure. This job does not certify mission/interactive support.\n"
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as stream:
            stream.write(text)
    print(text)
    # Workflows require both successful step outcome and this final readiness flag,
    # and upload only this fresh, validated subdirectory.
    step_output("directory", str(output.resolve()))
    step_output("ready", "true")


if __name__ == "__main__":
    main()
