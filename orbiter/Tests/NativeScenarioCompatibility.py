"""Focused real-loader checks from stock Windows' bounded raw-save evidence."""
from pathlib import Path
import ctypes
import hashlib
import json
import subprocess
import sys
import tempfile

from NativePDIProtocol import SCENARIO, field, require, vessels


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def clocks(text):
    ships = vessels(text)
    return {name: field(ships[name][1], "MISSNTIME") for name in ("Columbia", "Eagle")}


def clock_reproducer(reference):
    # Use the actual C conversions from the two loaders, not a tolerance or
    # a replacement mission clock. Ordinary integration equations stay intact.
    sscanf = ctypes.CDLL(None).sscanf
    # Apple arm64 distinguishes fixed and variadic arguments in its ABI.
    sscanf.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    sscanf.restype = ctypes.c_int
    result = {}
    for name, values in reference["saves"]["original"]["vessels"].items():
        token = values["MISSNTIME"].encode()
        single, double = ctypes.c_float(), ctypes.c_double()
        require(sscanf(token, b"%f", ctypes.byref(single)) == 1, "float parse")
        require(sscanf(token, b"%lf", ctypes.byref(double)) == 1, "double parse")
        require(single.value != double.value, "reference must expose float quantization")
        result[name] = {"source_token": token.decode(), "upstream_float_s": single.value,
                        "native_double_s": double.value,
                        "upstream_minus_native_s": single.value - double.value}
    return result


def main():
    require(len(sys.argv) == 6, "probe, build, data, textures and evidence directory required")
    probe, build, data = map(Path, sys.argv[1:4])
    textures = sys.argv[4]
    output = Path(sys.argv[5])
    engine = Path(__file__).resolve().parents[1]
    fixture = engine / "Tests/Fixtures/NativeScenarioCompatibility"
    reference = json.loads((fixture / "windows-reference.json").read_text())
    output.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    report = {"gpu": False, "vm_operation": False, "reference": reference,
              "probe_sha256": sha(probe), "clock_reproducer": clock_reproducer(reference),
              "raw_evidence_directory": str(run), "checks": {}, "passed": False}
    try:
        root = run / "runtime"
        staging = [sys.executable, str(engine / "tools/stage_nassp_mission.py"), str(build),
                   "--addon", str(data), "--profile", "v9",
                   "--scenario", SCENARIO + ".scn", "--output", str(root)]
        if textures:
            staging += ["--textures", textures]
        subprocess.run(staging, check=True, stdout=subprocess.DEVNULL)
        original = root / "Scenarios" / (SCENARIO + ".scn")
        require(sha(original) == reference["saves"]["original"]["raw_sha256"],
                "unchanged original scenario bytes required")
        require(sha(fixture / "Orbiter.cfg") == reference["config_sha256"], "raw reference configuration")
        reference_config = (fixture / "Orbiter.cfg").read_bytes()
        require(reference_config.count(b"StartPaused = TRUE") == 1, "reference startup policy")
        # This headless driver owns every counted step and tests zero-step loads.
        # Stock StartPaused performs an initial step; exercise that separately.
        (root / "Orbiter.cfg").write_bytes(reference_config.replace(
            b"StartPaused = TRUE", b"StartPaused = FALSE") +
            b"\r\nFixedStep = 0.016666666666666666\r\n")
        report["runtime_startup_policy"] = "StartPaused FALSE; explicit driver owns all steps and zero-step reload"

        def execute(name, scenario, steps):
            command = [str(probe), str(root), scenario, str(steps), "--pause-after"]
            result = subprocess.run(command, capture_output=True, text=True, timeout=120)
            (run / (name + ".log")).write_text(result.stdout + result.stderr)
            require(result.returncode == 0, name + " real host execution failed")
            saves = {p.stem: p.read_text() for p in (root / "Scenarios").glob("Parity*.scn")}
            for p in (root / "Scenarios").glob("Parity*.scn"):
                (run / (name + "-" + p.name)).write_bytes(p.read_bytes())
            (run / (name + "-trace.jsonl")).write_bytes((root / "parity-state.jsonl").read_bytes())
            return saves

        states = execute("coast", SCENARIO, 301)
        original_clocks = clocks(original.read_text())
        require({n: float(v) for n, v in clocks(states["ParityLoaded"]).items()} ==
                {n: float(v) for n, v in original_clocks.items()}, "zero-step mission-clock precision")
        report["checks"]["zero_step_clocks_preserved"] = True
        for label in ("Loaded", "Baseline", "Coast"):
            csm = vessels(states["Parity" + label])["Columbia"][1]
            actual = field(csm, "DATARECORDER")
            expected = ("15.000000 15.000000 0 0 0 0 0 0 0 0 0" if label == "Loaded" else
                        reference["saves"][label.lower()]["vessels"]["Columbia"]["DATARECORDER"])
            report["checks"]["recorder_" + label.lower()] = {"actual": actual, "expected": expected,
                                                             "equal": actual == expected}
        require(all(v["equal"] for k, v in report["checks"].items() if k.startswith("recorder_")),
                "legacy DSE must use constructor defaults for missing relays and match Windows coast")

        # Reload the complete modern record, including asserted relay flags;
        # do not replace a valid save with the legacy defaults.
        (root / "Scenarios/ReloadSeed.scn").write_text(states["ParityCoast"])
        reload = execute("reload", "ReloadSeed", 0)["ParityLoaded"]
        report["checks"]["modern_recorder_retained"] = (
            field(vessels(reload)["Columbia"][1], "DATARECORDER") ==
            field(vessels(states["ParityCoast"])["Columbia"][1], "DATARECORDER"))
        require(report["checks"]["modern_recorder_retained"], "complete v9 recorder flags must survive reload")
        report["checks"]["reload_clocks_retained"] = clocks(reload) == clocks(states["ParityCoast"])
        require(report["checks"]["reload_clocks_retained"], "saved mission clocks must survive zero-step reload")
        report["passed"] = True
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (run / "result.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"passed": report["passed"], "evidence": str(run),
                          "clock_reproducer": report["clock_reproducer"]}), flush=True)


if __name__ == "__main__":
    main()
