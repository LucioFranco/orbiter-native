#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run a bounded native Windows CPU/module/NASSP smoke subset."""
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "orbiter/build/ci-windows"
TESTS = ("portable_sdk", "native_core", "module_lifecycle", "native_world",
         "nassp_mesh", "panelsdk_math", "nassp_scenario_state", "native_audio",
         "nassp_stage_solar", "native_solar", "native_nassp_resume",
         "nassp_stage_original_insertion")


def main():
    registered = {item["name"] for item in json.loads(subprocess.check_output(
        ["ctest", "--test-dir", str(BUILD), "--show-only=json-v1"], text=True))["tests"]}
    missing = set(TESTS) - registered
    if missing:
        raise SystemExit("Required Windows native tests are not registered: " + ", ".join(sorted(missing)))
    (BUILD / "test-plan.json").write_text(json.dumps({
        "suite": "windows-hosted-smoke", "full_acceptance": False,
        "selected": TESTS, "excluded": "All other registered tests need separate platform validation",
        "known_strict_gate": "native_pdi_pilot; historical FAIL; see acceptance-context.json"
    }, indent=2) + "\n")
    receipt = BUILD / "test-result.json"
    report = BUILD / "ctest.xml"
    receipt.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    command = ["ctest", "--test-dir", str(BUILD), "--output-on-failure", "--no-tests=error",
               "--parallel", "1", "--timeout", "180", "--output-junit", str(report),
               "--fixture-exclude-any", ".*", "-R",
               "^(" + "|".join(re.escape(name) for name in TESTS) + ")$"]
    result = subprocess.run(command)
    counts = {"scheduled": len(TESTS)}
    if report.is_file():
        xml = ET.parse(report).getroot()
        counts.update({key: int(xml.attrib.get(key, 0)) for key in
                       ("tests", "failures", "errors", "skipped")})
    receipt.write_text(json.dumps({"suite": "windows-hosted-smoke",
                                   "status": "passed" if result.returncode == 0 else "failed",
                                   "ctest_returncode": result.returncode, "counts": counts}, indent=2) + "\n")
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
