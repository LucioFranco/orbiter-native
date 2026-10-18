"""Exact serialized LM persistence checks and bounded continuation comparisons."""
import math
import re
from NativePDIProtocol import require, vessels


def block(text, begin, end):
    found = re.search(r"^" + re.escape(begin) + r"\s*\n(.*?)^" + re.escape(end) + r"\s*$", text, re.M | re.S)
    require(found is not None, "missing persistence block " + begin)
    return {line.split()[0]: line.split()[1:] for line in found[1].splitlines() if line.split()}


def persistence(text):
    lm = vessels(text)["Eagle"][1]
    radar = block(lm, "LEM_LR_START", "LEM_LR_END")
    result = {"radar": {k: [float(v) for v in radar[k]] for k in
                         ("RANGE", "ANTENNAANGLE", "RANGEGOOD", "VELOCITYGOOD", "RATE")}}
    for side in ("LEFT", "RIGHT"):
        found = re.findall(r"^\s*CROSSPOINTER_" + side + r"\s+([^\n]+)$", lm, re.M)
        require(len(found) == 1, "one serialized " + side + " crosspointer required")
        result["crosspointer_" + side.lower()] = [float(v) for v in found[0].split()]
    for axis in ("PITCH", "ROLL"):
        values = block(lm, "DPSGIMBALACTUATOR_" + axis + "_BEGIN", "DPSGIMBALACTUATOR_END")
        result["gimbal_" + axis.lower()] = {k: [float(v) for v in values[k]] for k in
                                         ("POSITION", "COMMANDEDPOSITION", "MOTORRUNNING")}
    dps = block(lm, "DPS_BEGIN", "DPS_END")
    require("THRUSTCOMMAND" in dps, "new checkpoint must explicitly preserve DPS THRUSTCOMMAND; old saves cannot prove faithful resume")
    result["dps"] = {key: [float(v) for v in values] for key, values in dps.items()}
    state = re.findall(r"^\s*MASSCG_STATE\s+([^\n]+)$", lm, re.M)
    require(len(state) == 1, "new checkpoint must preserve cached CoG/PMI/fuel history")
    result["mass_cg_state"] = [float(v) for v in state[0].split()]
    require(len(result["mass_cg_state"]) == 7, "complete cached CoG/PMI/fuel history required")
    return result


def persistence_differences(before, after):
    a, b = persistence(before), persistence(after)
    differences = []
    for key in a:
        if isinstance(a[key], dict):
            differences.extend(key + "." + field for field in a[key] if a[key][field] != b[key][field])
        elif a[key] != b[key]:
            differences.append(key)
    return differences


def verify_persistence(before, after):
    differences = persistence_differences(before, after)
    require(not differences, "zero-step LM persistence changed: " + ", ".join(differences))
    return persistence(before)


# Bounds derived from SDK scenario output precision, with a small integration
# allowance over 0.5 s. These are observers, not Apollo flight requirements.
# RPOS .001 m and AROT .001 degree give <.04 ft range error here (H<2000 ft,
# beam cosine >.98). VROT .0001 deg/s gives <1e-6 rad/s per component;
# twice that bound covers half a second without accepting cold-start impulses.
LIMITS = {"range_ft": .05, "angular_rate_rad_s": 2e-6,
          "rotation_component": 5e-5, "thrust_level": 1e-6,
          "gimbal_position": 2e-12}


def continuation_comparison(reference, resumed):
    a = [e for e in reference if e["event"] == "neutral_resume_sample"]
    b = [e for e in resumed if e["event"] == "neutral_resume_sample"]
    require(len(a) == len(b) == 31, "exactly 31 neutral continuation samples required")
    violations, maxima = [], {key: 0.0 for key in LIMITS}
    def bounded(key, x, y, tick):
        delta = abs(x-y)
        require(math.isfinite(delta), "finite continuation observations required")
        maxima[key] = max(maxima[key], delta)
        if delta > LIMITS[key]:
            violations.append({"tick": tick, "field": key, "difference": delta, "limit": LIMITS[key]})
    for tick, (x, y) in enumerate(zip(a, b)):
        require(x["action"] == "uninterrupted" and y["action"] == "resumed" and
                x["neutral_trace_tick"] == y["neutral_trace_tick"] == tick,
                "paired uninterrupted/resumed observation ticks required")
        for e in (x, y):
            require(not e["ground_contact"] and e["lgc_altitude_ft"] > 1500 and
                    e["pilot_aca_axis_s16"] == 0 and not e["aca_out_of_detent"] and
                    e["aca_proportional_roll_pitch_yaw"] == [0, 0, 0],
                    "bounded neutral original-ACA handoff required")
        require(abs((x["mission_time_s"]-a[0]["mission_time_s"])-tick/60) < 1e-5 and
                abs(x["mission_time_s"]-y["mission_time_s"]) < 1e-5,
                "paired continued mission clocks required")
        for key in ("RANGEGOOD", "VELOCITYGOOD", "ANTENNAANGLE"):
            if x["radar"][key] != y["radar"][key]:
                violations.append({"tick": tick, "field": "radar."+key})
        for key in ("range_good_discrete", "velocity_good_discrete", "position1_discrete", "position2_discrete"):
            if x["radar_geometry"][key] != y["radar_geometry"][key]:
                violations.append({"tick": tick, "field": key})
        if x["rcs_jet_levels"] != y["rcs_jet_levels"]:
            violations.append({"tick": tick, "field": "rcs_jet_levels"})
        if x["crosspointers"] != y["crosspointers"]:
            violations.append({"tick": tick, "field": "both_crosspointers"})
        for g, h in zip(x["dps_gimbals"], y["dps_gimbals"]):
            for key in ("command", "motor_running"):
                if g[key] != h[key]:
                    violations.append({"tick": tick, "field": "gimbal."+key})
            bounded("gimbal_position", g["position"], h["position"], tick)
        bounded("range_ft", float(x["radar"]["RANGE"]), float(y["radar"]["RANGE"]), tick)
        bounded("thrust_level", x["dps_level"], y["dps_level"], tick)
        for u, v in zip(x["angular_velocity_body_rad_s"], y["angular_velocity_body_rad_s"]):
            bounded("angular_rate_rad_s", u, v, tick)
        for r, s in zip(x["rotation_body_to_global"], y["rotation_body_to_global"]):
            for u, v in zip(r, s):
                bounded("rotation_component", u, v, tick)
    return {"continuation_matches_observation_bounds": not violations, "samples": 31, "duration_seconds": .5,
            "bounds": LIMITS, "maximum_differences": maxima, "violations": violations,
            "first_step": {"reference": a[1], "resumed": b[1]}}



def processor_persistence(before, after):
    counts = [int(block(vessels(text)["Eagle"][1], "AGC_BEGIN", "AGC_END")["CYCLECOUNTER"][0])
              for text in (before, after)]
    return {"saved_cycle_counter": counts[0], "reloaded_cycle_counter": counts[1],
            "cycle_counter_retained": counts[0] == counts[1],
            "cdu_fifo_snapshot_supported": False,
            "source_limit": "Pinned ApolloGuidance::LoadState deliberately skips CYCLECOUNTER until CDU FIFOs are saved/loaded; yaAGC stores those queues in module-static state shared by same-class AGCs"}
