#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline pin, scope and syntax checks; source-tree failures remain fatal."""
import argparse
import ast
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="Read-only source tree for validating an unintegrated overlay")
    args = parser.parse_args()
    source = args.source_root
    inputs = json.loads((ROOT / "ci/inputs.json").read_text())
    cmake = "\n".join(p.read_text() for p in (source / "orbiter/cmake").glob("Native*.cmake"))
    cmake += (source / "orbiter/Extern/Lua/CMakeLists.txt").read_text()
    for name, item in inputs["dependencies"].items():
        assert re.fullmatch("[a-f0-9]{40}", item["commit"]), name
        assert item["repository"] in cmake and item["commit"] in cmake, "CI/CMake pin drift: " + name
    engine = json.loads((source / "orbiter/upstream/openorbiter-native.json").read_text())
    data = json.loads((source / "orbiter/upstream/nassp-data-v9.json").read_text())
    assert engine["upstream"]["commit"] == inputs["openorbiter"]
    assert data["commit"] == inputs["nassp"]
    context = json.loads((ROOT / "ci/acceptance-context.json").read_text())
    evidence = json.loads((source / context["evidence"]).read_text())
    assert context["counts"] == evidence["counts"]
    assert context["tested_source_commit"] == evidence["tested_source_commit"]
    assert context["known_strict_failed_tests"] == evidence["known_strict_failed_tests"]
    assert context["not_a_current_job_result"] is True and context["preview_ready"] is False
    workflow = (ROOT / ".github/workflows/native-foundation.yml").read_text()
    refs = re.findall(r"uses:\s*([^\s]+)", workflow)
    assert refs and all(re.fullmatch(r"[\w.-]+/[\w./-]+@[a-f0-9]{40}", ref) for ref in refs)
    assert "github.repository == 'LucioFranco/orbiter-native'" in workflow
    assert "github.event.repository.private == false" in workflow
    assert workflow.count("needs: policy") == 3
    assert "permissions:\n  contents: read" in workflow
    assert workflow.count("persist-credentials: false") == 4
    assert workflow.count("retention-days: 7") == 3
    assert workflow.count("id: collect_reports") == 3
    assert workflow.count("if: always() && steps.collect_reports.outcome == 'success' && steps.collect_reports.outputs.ready == 'true'") == 3
    assert workflow.count("path: ${{ steps.collect_reports.outputs.directory }}") == 3
    assert "path: ci-reports/" not in workflow
    assert "if: success() && github.event_name == 'push'" in workflow
    assert not any(s in workflow for s in ("secrets.", "pull_request_target", "continue-on-error",
                                         "contents: write", "id-token: write", "self-hosted",
                                         "gh release", "dist/", "upload-pages"))
    linux = (source / ".github/workflows/linux-runtime.yml").read_text()
    linux_refs = re.findall(r"uses:\s*([^\s]+)", linux)
    assert linux_refs and all(re.fullmatch(r"[\w.-]+/[\w./-]+@[a-f0-9]{40}", ref) for ref in linux_refs)
    assert "github.repository == 'LucioFranco/orbiter-native'" in linux
    assert "github.event.repository.private == false" in linux
    assert "permissions:\n  contents: read" in linux
    assert "persist-credentials: false" in linux
    assert "python3 ci/fetch_inputs.py --nassp-data" in linux
    assert "python3 ci/build_native.py --nassp" in linux
    assert "python3 packaging/linux/package.py" in linux
    assert linux.count("inputs.package_linux == true") == 2
    clearance = json.loads((source / "packaging/linux/clearance.json").read_text())
    assert clearance["release_enabled"] is False
    assert clearance["checks"] and all(value is False for value in clearance["checks"].values())
    assert not any(s in linux for s in ("secrets.", "pull_request_target", "continue-on-error",
                                      "contents: write", "id-token: write", "self-hosted", "gh release"))
    for folder in ("ci", "packaging"):
        for path in (ROOT / folder).rglob("*.py"):
            ast.parse(path.read_text(), filename=str(path))
    print("PASS: v9 source/data/dependency pins, immutable action references, public-only guards, package gate and Python syntax")


if __name__ == "__main__":
    main()
