"""Exercise user-facing preflight failures without changing source data or launching a demo."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("data", type=Path)
parser.add_argument("profile", choices=("baseline", "candidate", "v9"))
parser.add_argument("source", type=Path)
parser.add_argument("--orbiter-textures", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
helper = root / "tools/nassp_demo.py"
data, source, build = (p.resolve() for p in (args.data, args.source, args.build))
checks = []

def run(action="check-data", data_root=data, profile=args.profile, extra=(), expected=None):
    result = subprocess.run([sys.executable, str(helper), action, "--nassp-data-root", str(data_root),
                             "--profile", profile, *map(str, extra)], cwd=root, text=True, capture_output=True)
    if expected:
        assert result.returncode == 2, (result.returncode, result.stderr)
        assert expected in result.stderr, result.stderr
        assert "Traceback" not in result.stderr, result.stderr
        checks.append(expected)
        return None
    assert result.returncode == 0, result.stderr
    return json.loads(result.stdout)

valid = run(extra=("--source-addon", source))
data_manifest = json.loads((root / "upstream" / ("nassp-data-" + args.profile + ".json")).read_text())
stack = {"baseline": "nassp", "candidate": "nassp-candidate", "v9": "nassp-v9"}[args.profile]
source_manifest = json.loads((root / "upstream" / stack / "manifest.json").read_text())
lists_manifest = json.loads((root / "upstream/nassp-manifests" / args.profile / "manifest.json").read_text())
assert valid["source_adaptations_verified"] == len(source_manifest["files"])
assert valid["critical_hashes_verified"] == len(data_manifest["critical"])
assert valid["asset_files_present"] == len(data_manifest["assets"])
assert valid["exported_source_lists_verified"] == len(lists_manifest["files"])
assert run(profile="auto")["profile"] == args.profile
checks.append("matching pinned data and adapted source")
other = "candidate" if args.profile == "baseline" else "baseline"
run(profile=other, expected="NASSP data hash mismatch")
run(profile=args.profile, extra=("--source-addon", root), expected="Adapted NASSP source mismatch")

with tempfile.TemporaryDirectory(prefix="nassp-preflight-") as temporary:
    temporary = Path(temporary)
    output = temporary / "untouched-runtime"
    if args.profile == "v9":
        source_fixture = temporary / "source"
        relative_lists = ["Orbitersdk/samples/ProjectApollo/" + name for name in lists_manifest["files"]]
        for name in {entry["path"] for entry in source_manifest["files"]} | set(relative_lists):
            target = source_fixture / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.symlink_to(source / name)
        selected_list = next(name for name in relative_lists if name.endswith("CMakeLists.txt"))
        target = source_fixture / selected_list
        original = target.read_bytes()
        target.unlink()
        target.write_bytes(original + b"\n# changed source list\n")
        run(extra=("--source-addon", source_fixture), expected="Adapted NASSP source-list mismatch")
    run("stage", data_root=temporary / "missing", extra=("--output", output), expected="NASSP data root is missing")
    assert not output.exists()
    fixture = temporary / "data"
    manifest = json.loads((root / "upstream" / ("nassp-data-" + args.profile + ".json")).read_text())
    for name in set(manifest["critical"]) | set(manifest["assets"]):
        target = fixture / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.symlink_to(data / name)
    selected = next(iter(manifest["critical"]))
    (fixture / selected).unlink()
    run(data_root=fixture, expected="Incomplete NASSP data: missing " + selected)
    (fixture / selected).symlink_to(data / selected)
    scenario = next(name for name in manifest["critical"] if name.endswith("Insertion.scn"))
    for name in ("Config/ProjectApollo/LEMSystems.cfg", "Config/ProjectApollo/Comanche055.bin", scenario):
        target = fixture / name
        original = (data / name).read_bytes()
        target.unlink()  # Never write through a symlink into the supplied data.
        target.write_bytes(original + b"corrupt")
        run("stage", data_root=fixture, extra=("--output", output), expected="NASSP data hash mismatch: " + name)
        assert not output.exists()
        target.unlink()
        target.symlink_to(data / name)
    run(extra=("--orbiter-textures", temporary / "textures"), expected="Missing planetary texture/elevation archive")
    fake_build = temporary / "build"
    fake_build.mkdir()
    cache = fake_build / "CMakeCache.txt"
    cache.write_text("CMAKE_HOME_DIRECTORY:INTERNAL=" + str(temporary / "old-engine") + "\n")
    run("check", extra=("--build", fake_build), expected="This build belongs to another engine checkout")
    cache.write_text("CMAKE_HOME_DIRECTORY:INTERNAL=" + str(root) + "\nNASSP_ADDON_DIR:PATH=" + str(source) + "\nNASSP_DATA_DIR:PATH=" + str(data) + "\n")
    run("check", extra=("--build", fake_build), expected="Native executable is missing")
    cache.write_text(cache.read_text().replace(str(data), str(fixture)))
    run("check", extra=("--build", fake_build), expected="Build NASSP_DATA_DIR differs")

extra = ["--build", build]
if args.orbiter_textures:
    extra += ["--orbiter-textures", args.orbiter_textures]
report = run("check", extra=extra)
assert not Path(report["launch_argv"][0]).is_absolute(), report["launch_command"]
assert not Path(report["launch_argv"][2]).is_absolute(), report["launch_command"]
checks.append("checkout-relative executable and runtime in launch command")
run("stage", extra=(*extra, "--output", data), expected="Runtime output must be a separate directory")
run("stage", extra=(*extra, "--output", data / "runtime"), expected="Runtime output must not be inside a NASSP data")
run("stage", extra=(*extra, "--source-addon", root), expected="--source-addon differs")
with tempfile.TemporaryDirectory(prefix="nassp-reused-output-") as directory:
    output=Path(directory)/"runtime"
    stage_args=(*extra,"--output",output)
    staged=run("stage",extra=stage_args)
    provenance=staged["runtime_provenance"]
    assert Path(provenance["bindings"]["Modules"])==build/"runtime/Modules"
    assert provenance["asset_file_links_verified"]>=1054
    assert provenance["binary_sha256"]["orbiter"]
    foreign=(Path(directory)/"foreign").resolve();foreign.mkdir()
    for name in ("Modules","Resources"):
        (output/name).unlink();(output/name).symlink_to(foreign,target_is_directory=True)
    mesh=output/"Meshes/ProjectApollo/LM_VC.msh"
    mesh.unlink();mesh.write_bytes(b"foreign user-owned mesh")
    run("stage",extra=stage_args,expected="Runtime asset collision")
    assert mesh.read_bytes()==b"foreign user-owned mesh", "collision must not be overwritten"
    assert (output/"Modules").resolve()==foreign,"collision must fail before other output changes"
    mesh.unlink();mesh.symlink_to(foreign/"stale.msh")
    refreshed=run("stage",extra=stage_args)
    assert (output/"Modules").resolve()==build/"runtime/Modules"
    assert (output/"Resources").resolve()==build/"runtime/Resources"
    assert mesh.resolve()==data/"Meshes/ProjectApollo/LM_VC.msh"
    assert refreshed["runtime_provenance"]["binary_sha256"]==provenance["binary_sha256"]
    checks.extend(["reused output refreshes foreign binary/resource/asset links",
        "actual staged binary hashes and resolved asset roots recorded"])
print(json.dumps({"profile": args.profile, "checks_passed": len(checks), "checks": checks}, indent=2))
