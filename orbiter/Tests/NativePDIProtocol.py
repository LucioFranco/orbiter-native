"""Verify an observational Mac PDI run ending before the first V57 decision."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import subprocess
import time

SCENARIO = "Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m"
N68_KEYS = [3401, 3406, 3411, 3402, 3411, 3413, 3418]
KEY_INPUTS = {
    "initial_auto_maneuver_pro": [3416], "final_trim_pro": [3416],
    "final_trim_enter": [3418], "v77": [3401, 3412, 3412, 3418],
    "ignition_pro": [3416], "request_n68": N68_KEYS,
}
FINAL_CONTROLS = {
    "ModeControlPGNSSwitch": 2, "ModeControlAGSSwitch": 2,
    "EngineArmSwitch": 0, "EngineDescentCommandOverrideSwitch": 1,
    "LandingAntSwitch": 2, "PGNS_LDG_RDR_CB": 1, "RateScaleSwitch": 1,
    "AltRngMonSwitch": 0, "RendezvousRadarRotary": 1,
    "ORDEALFDAI1Switch": 0, "ORDEALFDAI2Switch": 0,
}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def signed_word(word):
    require(0 <= word <= 0o77777, "invalid AGC word")
    return word - 0o77777 if word & 0o40000 else word


def agc_seconds(high, low):
    """Two AGC signed 15-bit words, weighted by 2**14, in centiseconds."""
    return (signed_word(high) * 16384 + signed_word(low)) / 100


def vessels(text):
    ships = text.split("BEGIN_SHIPS", 1)[1].split("END_SHIPS", 1)[0]
    return {name: (cls.replace("\\", "/"), state) for name, cls, state in
            re.findall(r"^([^\s:]+):([^\n]+)\n(.*?)^END$", ships, re.M | re.S)}


def field(text, key):
    match = re.search(r"^\s*" + re.escape(key) + r"\s+([^\n]+)", text, re.M)
    require(match is not None, "missing scenario field " + key)
    return match[1]


def agc(state):
    block = state.split("AGC_BEGIN", 1)[1].split("AGC_END", 1)[0]
    return [line for line in block.splitlines() if not line.strip().startswith("CYCLECOUNTER ")]


def preflight(source, runtime):
    source, runtime = Path(source), Path(runtime)
    staged = runtime / "Scenarios" / (SCENARIO + ".scn")
    require(staged.read_bytes() == source.read_bytes(), "unchanged supplied scenario required")
    ships = vessels(source.read_text())
    require(ships["Eagle"][0] == "ProjectApollo/LEM", "original Eagle required")
    eagle = ships["Eagle"][1]
    require(field(eagle, "APOLLONO") == "11", "Apollo 11 required")
    require((field(eagle, "PROG"), field(eagle, "VERB"), field(eagle, "NOUN")) ==
            ("63", "50", "18"), "original P63 V50 N18 required")
    data = source.parents[4]
    inputs = {"mission": Path("Missions/ProjectApollo/Apollo 11.cfg"),
              "rom": Path("Config/ProjectApollo/Luminary099.bin"),
              "checklist": Path("Doc/Project Apollo - NASSP/Checklists/Apollo 11 LM Checklists.xls")}
    require(re.search(r"^LGCVersion=Luminary099\s*$", (runtime / inputs["mission"]).read_text(), re.M),
            "Luminary099 required")
    hashes = {}
    for name, relative in inputs.items():
        expected, supplied = data / relative, runtime / relative
        require(supplied.read_bytes() == expected.read_bytes(), "matching supplied " + name + " required")
        hashes[name] = hashlib.sha256(supplied.read_bytes()).hexdigest()
    require("FILE Doc\\Project Apollo - NASSP\\Checklists\\Apollo 11 LM Checklists.xls" in eagle,
            "scenario's original LM checklist selection required")
    block = eagle.split("AGC_BEGIN", 1)[1].split("AGC_END", 1)[0]
    words = {int(a, 8): int(v, 8) for a, v in re.findall(r"^\s*EMEM([0-7]+) ([0-7]+)$", block, re.M)}
    clock = agc_seconds(words.get(0o24, 0), words.get(0o25, 0))
    tig = agc_seconds(words.get(0o3441, 0), words.get(0o3442, 0))
    require(850 < tig - clock < 950, "original PDI-minus-15-minute clock/TIG required")
    return {"scenario": SCENARIO + ".scn", "scenario_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "input_sha256": hashes, "initial_agc_clock_s": clock, "initial_tig_s": tig,
            "initial_countdown_s": tig - clock, "vessels": sorted(ships), "scenario_modified": False}


# Diagnostic observer criteria only; neither a flight requirement nor dH limits.
YAW_RATE_THRESHOLD_DEG_S = 0.1
YAW_RATE_DWELL_S = 1.0


def matrix_product(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def transpose(a):
    return [list(row) for row in zip(*a)]


def rotation_distance_deg(a, b):
    relative = matrix_product(transpose(a), b)
    return math.degrees(math.acos(max(-1.0, min(1.0, (sum(relative[i][i] for i in range(3)) - 1) / 2))))


def right_yaw_matrix(degrees):
    # Orbiter uses left-handed vessel axes: recorded right yaw (-body Y rate)
    # advances this positive matrix angle. Check against the recorded matrices.
    angle = math.radians(degrees)
    c, s = math.cos(angle), math.sin(angle)
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def validate_rotation(matrix):
    require(len(matrix) == 3 and all(len(row) == 3 for row in matrix), "3x3 attitude matrix required")
    require(all(isinstance(v, (int, float)) and math.isfinite(v) for row in matrix for v in row),
            "finite attitude matrix required")
    product = matrix_product(transpose(matrix), matrix)
    require(max(abs(product[i][j] - int(i == j)) for i in range(3) for j in range(3)) < 1e-6,
            "orthonormal attitude matrix required")
    determinant = sum(matrix[0][j] * (matrix[1][(j+1)%3] * matrix[2][(j+2)%3] -
                                    matrix[1][(j+2)%3] * matrix[2][(j+1)%3]) for j in range(3))
    require(abs(determinant - 1) < 1e-6, "proper attitude rotation required")


def analyze_yaw_settling(events, action, target_degrees):
    def one(kind):
        found = [e for e in events if e["event"] == kind and e["action"] == action]
        require(len(found) == 1, "one yaw " + kind + " required")
        return found[0]
    down, up, released = (one(kind) for kind in ("keyboard_down", "keyboard_up", "yaw_input_completed"))
    trace = [e for e in events if e["event"] == "yaw_settling_sample" and e["action"] == action]
    require(len(trace) == 480, "480 per-step post-release observations required")
    require(all(abs(e["sim_elapsed_s"] - up["sim_elapsed_s"] - (i+1)/60) < 1e-8
                for i, e in enumerate(trace)), "continuous eight-second post-release trace required")
    require(abs(released["sim_elapsed_s"] - up["sim_elapsed_s"] - .2) < 1e-8,
            "release verification is the 0.2-second observation")
    points = [e for e in events if down["seq"] <= e["seq"] <= trace[-1]["seq"]]
    for e in points:
        validate_rotation(e["rotation_body_to_global"])
    for e in trace:
        require(not e["keyboard_yaw_right_down"] and e["aca_keyboard_yaw_right"] == 0
                and not e["aca_out_of_detent"] and abs(e["aca_proportional_roll_pitch_yaw"][2]) < 1e-12
                and not (e["guidance_channel_031"] & ((1 << 14) | (1 << 3) | (1 << 2))),
                "neutral original input must remain distinct from rate settling")
        for key in ("pgns_jet_requests", "jet_driver", "jet_requests", "rcs_jet_levels"):
            require(len(e[key]) == 16 and all(0 <= value <= 1 for value in e[key]), "16 original jet values required")
        if e["atca_primary_voltage_v"] > 20 and not e["guidance_relay_k12"]:
            requests = [0] * 16
            for channel, jets in ((5, [12, 15, 8, 11, 4, 7, 0, 3]), (6, [10, 13, 1, 6, 5, 9, 14, 2])):
                raw = e["lgc_output_channel_00" + str(channel)]
                require(isinstance(raw, int) and 0 <= raw <= 0o77777, "original LGC output word required")
                for bit, jet in enumerate(jets):requests[jet] = int(bool(raw & (1 << bit)))
            require(requests == e["pgns_jet_requests"], "LGC output channels must match original PGNS jet requests")
    start = down["rotation_body_to_global"]
    target = matrix_product(start, right_yaw_matrix(target_degrees))
    previous_angle = 0.0
    unwrapped = 0.0
    angles = {}
    for e in points:
        relative = matrix_product(transpose(start), e["rotation_body_to_global"])
        angle = math.degrees(math.atan2(relative[0][2] - relative[2][0], relative[0][0] + relative[2][2]))
        unwrapped += (angle - previous_angle + 180) % 360 - 180
        previous_angle = angle
        angles[e["seq"]] = unwrapped
    def observed(e):
        geometry = e["radar_geometry"]
        return {"sim_elapsed_s": e["sim_elapsed_s"], "after_release_s": e["sim_elapsed_s"] - up["sim_elapsed_s"],
                "yaw_rate_deg_s": -math.degrees(e["angular_velocity_body_rad_s"][1]),
                "relative_right_yaw_projection_unwrapped_deg": angles[e["seq"]],
                "orientation_error_to_fixed_start_frame_target_deg": rotation_distance_deg(target, e["rotation_body_to_global"]),
                "rotation_body_to_global": e["rotation_body_to_global"], "radar_geometry": geometry,
                "original_radar": e["radar"], "lgc_output_channel_005": e["lgc_output_channel_005"],
                "lgc_output_channel_006": e["lgc_output_channel_006"], "pgns_jet_requests": e["pgns_jet_requests"],
                "jet_requests": e["jet_requests"], "rcs_jet_levels": e["rcs_jet_levels"]}
    quiet_start = None
    quiet_first = None
    settled = None
    for e in trace:
        if abs(math.degrees(e["angular_velocity_body_rad_s"][1])) <= YAW_RATE_THRESHOLD_DEG_S:
            if quiet_start is None:quiet_start = quiet_first = e
            if e["sim_elapsed_s"] - quiet_start["sim_elapsed_s"] >= YAW_RATE_DWELL_S - 1e-8:
                settled = e
                break
        else:quiet_start = quiet_first = None
    post_release = [up] + trace
    extra_gyro = sum(-.5 * (a["angular_velocity_body_rad_s"][1] + b["angular_velocity_body_rad_s"][1]) *
                     (b["sim_elapsed_s"] - a["sim_elapsed_s"]) * 180 / math.pi
                     for a, b in zip(post_release, post_release[1:]))
    return {"input_release_verified": True, "rate_settling_observed": settled is not None,
            "rate_criterion": {"axis": "Orbiter body Y (LM yaw)", "absolute_rate_deg_s": YAW_RATE_THRESHOLD_DEG_S,
                               "continuous_dwell_s": YAW_RATE_DWELL_S, "diagnostic_only": True},
            "commanded_yaw_deg": target_degrees, "held_gyro_integral_deg": released["yaw_integrated_deg"],
            "post_release_eight_second_gyro_integral_deg": extra_gyro,
            "rotation_at_input_start": start, "fixed_start_frame_target_rotation": target,
            "release": observed(up), "release_verified_0_2s": observed(released),
            "first_low_rate_observation": observed(quiet_first) if settled else None,
            "rate_dwell_verified": observed(settled) if settled else None,
            "eight_second_observation": observed(trace[-1]),
            "settled_commanded_attitude_certified": False,
            "orientation_measurement": "Relative matrix rotation from input start; yaw projection is unwrapped, target is a fixed-start-frame original right-yaw rotation (negative body-Y rate, positive matrix angle). Ongoing PGNS pitch/roll guidance is not removed."}


# Relative-axis diagnostic only: no operational flight or dH requirement.
YAW_TARGET_TOLERANCE_DEG = 1.0


def rotation_quaternion(matrix):
    """Unit quaternion [w,x,y,z] from a proper body-to-global matrix."""
    validate_rotation(matrix)
    trace = sum(matrix[i][i] for i in range(3))
    if trace > 0:
        scale = 2 * math.sqrt(1 + trace)
        q = [scale / 4, (matrix[2][1]-matrix[1][2])/scale,
             (matrix[0][2]-matrix[2][0])/scale, (matrix[1][0]-matrix[0][1])/scale]
    else:
        i = max(range(3), key=lambda axis: matrix[axis][axis])
        j, k = (i+1)%3, (i+2)%3
        scale = 2 * math.sqrt(1 + matrix[i][i] - matrix[j][j] - matrix[k][k])
        q = [(matrix[k][j]-matrix[j][k])/scale, 0, 0, 0]
        q[i+1] = scale/4
        q[j+1] = (matrix[j][i]+matrix[i][j])/scale
        q[k+1] = (matrix[k][i]+matrix[i][k])/scale
    require(abs(sum(v*v for v in q)-1) < 1e-6, "unit attitude quaternion required")
    return q


def analyze_yaw_target(events, action, target_degrees):
    def one(kind):
        found = [e for e in events if e["event"] == kind and e["action"] == action]
        require(len(found) == 1, "one target yaw " + kind + " required")
        return found[0]
    start, acquired, end = (one(kind) for kind in
                           ("yaw_target_start", "yaw_target_acquired", "yaw_target_observation_end"))
    trace = [e for e in events if e["event"] == "yaw_target_sample" and e["action"] == action]
    require(trace and abs(trace[0]["sim_elapsed_s"]-start["sim_elapsed_s"]-1/60) < 1e-8,
            "target trace must start at first simulation step")
    require(all(abs(b["sim_elapsed_s"]-a["sim_elapsed_s"]-1/60) < 1e-8
                for a, b in zip(trace, trace[1:])), "continuous per-step target trace required")
    require(abs(end["sim_elapsed_s"]-acquired["sim_elapsed_s"]-8) < 1e-8 and
            abs(trace[-1]["sim_elapsed_s"]-end["sim_elapsed_s"]) < 1e-8,
            "eight-second neutral observation after target dwell required")
    keyboard = [e for e in events if e["action"] == action and e["event"] in ("keyboard_down", "keyboard_up")]
    require(2 <= len(keyboard) <= 24 and len(keyboard)%2 == 0 and
            all(e["event"] == ("keyboard_down" if i%2 == 0 else "keyboard_up") and e["area_id"] == 0x51
                for i, e in enumerate(keyboard)), "bounded right-only original ACA pulse pairs required")
    require(all(down["seq"] < up["seq"] and up["sim_elapsed_s"] > down["sim_elapsed_s"] and
                down["checklist_row"] == up["checklist_row"] == start["checklist_row"]
                for down, up in zip(keyboard[::2], keyboard[1::2])),
            "target pulse holds and original checklist row must be measured")
    require(end["sim_elapsed_s"]-start["sim_elapsed_s"] <= (90 if target_degrees < 100 else 110),
            "target maneuver duration must remain bounded")
    require(start["seq"] < keyboard[0]["seq"] and keyboard[-1]["seq"] < acquired["seq"],
            "target input/release/acquisition ordering required")
    previous_angle = unwrapped = 0.0
    angles = {}
    quaternions = {}
    points = [e for e in events if start["seq"] <= e["seq"] <= end["seq"]]
    for e in points:
        relative = matrix_product(transpose(start["rotation_body_to_global"]), e["rotation_body_to_global"])
        q = rotation_quaternion(relative)
        require(math.hypot(q[0], q[2]) > 1e-9, "relative yaw quaternion twist is singular")
        angle = math.degrees(2*math.atan2(q[2], q[0]))
        unwrapped += (angle-previous_angle+180)%360-180
        previous_angle = angle
        angles[e["seq"]] = unwrapped
        quaternions[e["seq"]] = q
    dwell = [e for e in trace if acquired["sim_elapsed_s"]-1-1e-8 <= e["sim_elapsed_s"] <= acquired["sim_elapsed_s"]]
    require(len(dwell) >= 61, "full one-second terminal target dwell required")
    neutral = [e for e in trace if e["sim_elapsed_s"] >= acquired["sim_elapsed_s"]-1-1e-8]
    for e in neutral + [acquired, end]:
        require(not e["keyboard_yaw_right_down"] and e["aca_keyboard_yaw_right"] == 0 and
                not e["aca_out_of_detent"] and abs(e["aca_proportional_roll_pitch_yaw"][2]) < 1e-12 and
                not (e["guidance_channel_031"] & ((1<<14)|(1<<3)|(1<<2))),
                "terminal target dwell requires neutral original ACA")
        require(abs(angles[e["seq"]]-target_degrees) <= YAW_TARGET_TOLERANCE_DEG,
                "terminal quaternion yaw must meet diagnostic target tolerance")
        require(abs(math.degrees(e["angular_velocity_body_rad_s"][1])) <= YAW_RATE_THRESHOLD_DEG_S,
                "terminal target dwell requires low measured yaw rate")
    def observed(e):
        return {"sim_elapsed_s": e["sim_elapsed_s"], "relative_quaternion_wxyz": quaternions[e["seq"]],
                "relative_right_yaw_twist_unwrapped_deg": angles[e["seq"]],
                "yaw_target_error_deg": angles[e["seq"]]-target_degrees,
                "yaw_rate_deg_s": -math.degrees(e["angular_velocity_body_rad_s"][1]),
                "full_rotation_error_to_pure_start_frame_yaw_deg": rotation_distance_deg(
                    matrix_product(start["rotation_body_to_global"], right_yaw_matrix(target_degrees)),
                    e["rotation_body_to_global"]),
                "rotation_body_to_global": e["rotation_body_to_global"],
                "original_radar": e["radar"], "radar_geometry": e["radar_geometry"]}
    return {"commanded_yaw_deg": target_degrees, "original_right_yaw_pulses": len(keyboard)//2,
            "target_frame": "Relative quaternion twist about input-start body Y (LM thrust/yaw axis); PGNS pitch/roll swing remains observed",
            "diagnostic_yaw_target_validated": True, "full_fixed_frame_attitude_certified": False,
            "diagnostic_criterion": {"yaw_angle_tolerance_deg": YAW_TARGET_TOLERANCE_DEG,
                "yaw_rate_threshold_deg_s": YAW_RATE_THRESHOLD_DEG_S, "continuous_dwell_s": YAW_RATE_DWELL_S,
                "operational_flight_requirement": False},
            "start": observed(start), "terminal_dwell": observed(acquired), "eight_second_observation": observed(end),
            "per_step_samples": len(trace), "original_input_events": [
                {k: e[k] for k in ("event", "area_id", "sim_elapsed_s", "checklist_row")} for e in keyboard]}


def analyze_pilot_yaw(events, action, target):
    """Independent body-rate integration, including original neutral braking."""
    def one(kind):
        found = [e for e in events if e["event"] == kind and e["action"] == action]
        require(len(found) == 1, "exactly one pilot " + kind + " required")
        return found[0]
    start, acquired, end = (one(k) for k in
                           ("pilot_yaw_start", "pilot_yaw_acquired", "pilot_yaw_observation_end"))
    samples = [e for e in events if e["event"] == "pilot_yaw_sample" and e["action"] == action]
    require(samples and start["seq"] < samples[0]["seq"] < acquired["seq"] < end["seq"],
            "continuous pilot observation order required")
    commands = [e for e in events if e["event"] == "pilot_axis_input" and e["action"] == action]
    require(commands and len(commands) % 2 == 0 and len(commands) <= 32,
            "bounded paired pilot axis commands required")
    require([e["pilot_aca_axis_s16"] for e in commands] == [14823, 0] * (len(commands)//2),
            "only declared proportional right yaw and neutral inputs allowed")
    previous, integrated = start, 0.0
    steady_counts = []
    for e in samples:
        delta = e["sim_elapsed_s"] - previous["sim_elapsed_s"]
        require(abs(delta - 1/60) < 1e-7, "pilot sample gap")
        integrated -= .5 * (previous["angular_velocity_body_rad_s"][1] +
                            e["angular_velocity_body_rad_s"][1]) * delta * 180/math.pi
        require(abs(integrated - e["yaw_integrated_deg"]) < 1e-6, "pilot body-rate integral mismatch")
        validate_rotation(e["rotation_body_to_global"])
        require(e["gasta_powered"] and all(math.isfinite(v) for v in e["pgns_fdai_rpy_deg"]),
                "original powered FDAI observation required")
        require(e["pilot_aca_axis_s16"] in (0,14823), "undeclared pilot axis input")
        if e["pilot_aca_axis_s16"] == 14823 and e["lgc_rhc_yaw_count"]:
            require(e["lgc_rhc_yaw_raw_s32"] == -20, "original raw signed RHC count readback required")
            steady_counts.append(e["lgc_rhc_yaw_count"])
        previous = e
    require(steady_counts and set(steady_counts) == {-19}, "original proportional RHC count readback required")
    require(abs(previous["sim_elapsed_s"]-end["sim_elapsed_s"]) < 1e-7, "pilot terminal sample required")
    dwell_start = acquired["sim_elapsed_s"] - 1
    dwell = [e for e in samples if e["sim_elapsed_s"] >= dwell_start - 1e-7]
    require(dwell and end["sim_elapsed_s"] - acquired["sim_elapsed_s"] >= 8-1e-7,
            "pilot full neutral dwell and observation required")
    require(all(e["pilot_aca_axis_s16"] == 0 and not e["aca_out_of_detent"] and
                abs(e["aca_proportional_roll_pitch_yaw"][2]) < 1e-9 and
                abs(e["yaw_integrated_deg"]-target) <= 1 and
                abs(e["angular_velocity_body_rad_s"][1]*180/math.pi) <= .1 for e in dwell),
            "pilot terminal body angle/rate/ACA neutral dwell required")
    require(not any(e["event"] == "keyboard_down" for e in events), "pilot driver must not mix keyboard yaw")
    require(start["sim_elapsed_s"] < acquired["sim_elapsed_s"] < end["sim_elapsed_s"] <=
            start["sim_elapsed_s"] + (90 if target == 10 else 110), "pilot maneuver duration bound")
    # Windows-up is an observable geometric orientation, not radar feedback.
    if target == 174:
        require(start["forward_window_zenith_cosine"] < 0 < end["forward_window_zenith_cosine"],
                "forward windows must change from down to up")
    relative = matrix_product(transpose(start["rotation_body_to_global"]), end["rotation_body_to_global"])
    q = rotation_quaternion(relative)
    twist = math.degrees(2*math.atan2(q[2],q[0]))
    if twist < 0: twist += 360
    return {"target_body_yaw_deg":target,"settled_body_yaw_deg": acquired["yaw_integrated_deg"],
            "post_observation_body_yaw_deg":integrated,"relative_matrix_twist_deg":twist,
            "pilot_axis_s16":14823,"original_lgc_rhc_counts":sorted(set(steady_counts)),
            "nominal_command_rate_deg_s":20*(46+4*19)*19/((46+4*42)*42),
            "peak_measured_right_yaw_rate_deg_s":max(-e["angular_velocity_body_rad_s"][1]*180/math.pi for e in samples),
            "acquired_elapsed_s":acquired["sim_elapsed_s"],"end_elapsed_s":end["sim_elapsed_s"],
            "forward_window_zenith_cosine_before":start["forward_window_zenith_cosine"],
            "forward_window_zenith_cosine_after":end["forward_window_zenith_cosine"],
            "fdai_rpy_deg_before":start["pgns_fdai_rpy_deg"],"fdai_rpy_deg_after":end["pgns_fdai_rpy_deg"],
            "observer_angle_tolerance_deg":1,"observer_rate_tolerance_deg_s":.1,
            "full_historical_pilot_pose_certified":False}


def validate_events(events, pilot_yaw=False):
    require(bool(events) and events[0]["event"] == "initial", "initial observation required")
    require(events[-1]["event"] == "checkpoint", "bounded decision checkpoint not reached")
    require(not any(e["event"] == "blocked" for e in events), "protocol was blocked")
    require(850 < events[0]["countdown_s"] < 950 and events[0]["dps_level"] == 0,
            "original pre-ignition clock required")
    require((events[0]["dsky"]["PROG"], events[0]["dsky"]["VERB"], events[0]["dsky"]["NOUN"]) ==
            ("63", "50", "18"), "original P63 V50 N18 observation required")
    yaw_enabled = events[0]["checklist_yaw_enabled"]
    target_enabled = events[0].get("yaw_target_driver_enabled", False)
    require(not target_enabled or yaw_enabled, "target driver requires declared yaw scope")
    require(events[0].get("pilot_driver_enabled", False) == pilot_yaw, "requested pilot scope must match log")
    require(not pilot_yaw or (yaw_enabled and not target_enabled), "pilot driver requires separate declared yaw scope")
    previous = -1.0
    mission_offset = events[0]["mission_time_s"] - events[0]["sim_elapsed_s"]
    for seq, e in enumerate(events):
        require(e["checklist_yaw_enabled"] == yaw_enabled, "consistent declared yaw scope required")
        require(e.get("yaw_target_driver_enabled", False) == target_enabled, "consistent target driver scope required")
        require(e.get("pilot_driver_enabled", False) == pilot_yaw, "consistent pilot scope required")
        require(e["seq"] == seq, "continuous event ordering required")
        for key in ("sim_elapsed_s", "mission_time_s", "agc_clock_s", "tig_s", "countdown_s", "lgc_tfi_s", "dps_level"):
            require(isinstance(e[key], (float, int)) and math.isfinite(e[key]), "finite " + key + " required")
        require(e["sim_elapsed_s"] >= previous, "simulation timestamps must not regress")
        require(abs(e["mission_time_s"] - e["sim_elapsed_s"] - mission_offset) < .01,
                "input holds must be included in the measured mission clock")
        require(abs(e["countdown_s"] - (e["tig_s"] - e["agc_clock_s"])) < .002, "TIG-minus-LGC-clock countdown required")
        require(abs(e["lgc_tfi_s"] - agc_seconds(int(e["tfi_high_octal"], 8), int(e["tfi_low_octal"], 8))) < .002,
                "TFI must decode the recorded LGC words")
        require(0 <= e["dps_level"] <= 1, "normalized DPS level required")
        require(not e["ground_contact"], "pre-V57 checkpoint cannot claim a landing")
        if e["event"] == "state_confirmed":
            require(e["readback"] == e["requested_state"], "original control readback mismatch")
        previous = e["sim_elapsed_s"]

    def one(kind, action):
        found = [e for e in events if e["event"] == kind and e["action"] == action]
        require(len(found) == 1, "exactly one " + action + " " + kind + " required")
        return found[0]

    def inputs(action):
        return [e for e in events if e["event"] == "input_down" and e["action"] == action]

    # Audit the complete DSKY command stream, including rejection of any hidden V57.
    dsky = [e for e in events if e["event"] == "input_down" and 3400 <= e["area_id"] <= 3418]
    require(all(e["action"] in KEY_INPUTS for e in dsky), "unapproved DSKY command")
    expected_stream = [(action, area) for action, areas in KEY_INPUTS.items() for area in areas]
    require([(e["action"], e["area_id"]) for e in dsky] == expected_stream,
            "complete DSKY command order required")
    releases = [e for e in events if e["event"] == "input_up" and 3400 <= e["area_id"] <= 3418]
    require([(e["action"], e["area_id"]) for e in releases] == expected_stream,
            "every DSKY input must be released")
    for down, up in zip(dsky, releases):
        hold = 2 if down["area_id"] == 3416 else .2
        require(down["seq"] < up["seq"] and abs(up["sim_elapsed_s"] - down["sim_elapsed_s"] - hold) < .02,
                "measured original DSKY input hold required")
    for action, expected in KEY_INPUTS.items():
        require([e["area_id"] for e in inputs(action)] == expected, "incorrect original key sequence " + action)
    for action, low, high in (("final_trim_pro", 239, 240.1), ("ignition_pro", 2.2, 5.1)):
        start = one("input_down", action)
        require(low < start["countdown_s"] <= high, "incorrect countdown timing " + action)
    entered = one("input_down", "final_trim_enter")
    returned = one("display_confirmed", "final_trim_return")
    n62 = one("display_confirmed", "n62_countdown")
    require(returned["dsky"]["VERB"] == "50" and returned["dsky"]["NOUN"] == "18",
            "final automatic maneuver return required")
    require(returned["seq"] < entered["seq"] < n62["seq"], "maneuver return must precede ENTER and N62")
    require(35 < entered["countdown_s"] < 240.1, "final trim ENTER timing")
    require(n62["dsky"]["NOUN"] == "62" and abs(n62["lgc_tfi_s"] + n62["countdown_s"]) < 2,
            "N62 must show the LGC's pre-ignition countdown")
    raw_tfi = re.fullmatch(r"([+-])(\d{2}) (\d{2})", n62["dsky"]["R2"])
    require(raw_tfi is not None and int(raw_tfi[3]) < 60, "complete N62 TFI register required")
    displayed_tfi = (int(raw_tfi[2]) * 60 + int(raw_tfi[3])) * (-1 if raw_tfi[1] == "-" else 1)
    require(abs(displayed_tfi - n62["lgc_tfi_s"]) < 2, "N62 displayed TFI must agree with recorded LGC TFI")
    require(n62["countdown_s"] > 182, "N62 must arrive before the planned V77/setup window")
    armed = one("state_confirmed", "EngineArmSwitch")
    require(33 < armed["countdown_s"] <= 35.1 and armed["readback"] == 0, "engine arming must wait until TIG-minus-35")
    ign = one("ignition", "first_positive_original_dps")
    require(ign["dps_level"] > 0 and all(e["dps_level"] == 0 for e in events[:ign["seq"]]),
            "ignition must be the first positive DPS observation, not the PRO event")
    require(abs(ign["actual_ignition_elapsed_s"] - ign["sim_elapsed_s"]) < 1e-5, "measured ignition timestamp required")
    require(one("input_up", "ignition_pro")["seq"] < ign["seq"], "ignition must follow the timed PRO input")
    require(all(e["actual_ignition_elapsed_s"] == ign["actual_ignition_elapsed_s"] for e in events[ign["seq"]:])
            and all(e["actual_ignition_elapsed_s"] is None for e in events[:ign["seq"]]),
            "actual ignition timestamp must remain fixed")
    n63 = one("display_confirmed", "ignition_n63")
    override = one("input_down", "EngineDescentCommandOverrideSwitch")
    require(n63["dsky"]["VERB"] == "06" and n63["dsky"]["NOUN"] == "63" and n63["seq"] < override["seq"],
            "worksheet row 150 N63 must precede override ON")
    require(ign["controls"]["EngineDescentCommandOverrideSwitch"] == 1 or
            int(ign["guidance_channel_011"]) & (1 << 12),
            "first DPS thrust requires original override or LGC engine-on readback")
    # The original override can start DPS before nominal LGC TIG. Preserve that
    # measured difference; do not replace it with either PRO time or LGC TIG.
    for action, seconds in (("tig_plus_26_seconds", 26), ("tig_plus_2m30s", 150),
                            ("tig_plus_3_minutes", 180), ("tig_plus_5_minutes", 300)):
        e = one("timing", action)
        require(seconds <= e["sim_elapsed_s"] - ign["sim_elapsed_s"] < seconds + .2,
                "post-ignition timing must use measured DPS ignition: " + action)
    request = one("n68_request", "request_n68")
    require(300 <= request["sim_elapsed_s"] - ign["sim_elapsed_s"] < 300.2, "first N68 request timing")
    last = events[-1]
    require(last["action"] == "v57_decision_pending_no_operational_dh_envelope" and last["checklist_row"] == 179,
            "must stop before the operational V57 decision")
    require(last["dsky"]["VERB"] == "16" and last["dsky"]["NOUN"] == "68", "N68 required at checkpoint")
    n68up = [e for e in events if e["event"] == "input_up" and e["action"] == "request_n68"][-1]
    require(request["seq"] < inputs("request_n68")[0]["seq"] and
            one("display_confirmed", "n68_display")["seq"] > n68up["seq"], "N68 command/display ordering required")
    require(last["sim_elapsed_s"] - n68up["sim_elapsed_s"] >= 5, "five-second N68 observation required")
    require(last["sim_elapsed_s"] <= 1500, "bounded run required")
    require(all(last["controls"][k] == v for k, v in FINAL_CONTROLS.items()), "checkpoint control configuration mismatch")
    raw_dh = last["dsky"]["R3"]
    dh = float(raw_dh) if re.fullmatch(r"[+-]?\d{5}", raw_dh) else None
    valid = last["radar"]["RANGEGOOD"] == "1" and last["radar"]["VELOCITYGOOD"] == "1"
    keyboard = [e for e in events if e["event"] in ("keyboard_down", "keyboard_up")]
    yaw_settling = {}
    yaw_targets = {}
    pilot_targets = {}
    if pilot_yaw:
        require(not keyboard and not any(e["event"] in ("yaw_input_completed", "yaw_settling_sample", "yaw_target_sample") for e in events),
                "pilot proportional input must remain separate from keyboard variants")
        for action, angle, row in (("pilot_yaw_right_10", 10, 55), ("pilot_yaw_right_174", 174, 169)):
            require(one("pilot_yaw_start", action)["checklist_row"] == row, "pilot checklist row required")
            pilot_targets[action] = analyze_pilot_yaw(events, action, angle)
        yaw_start = one("pilot_yaw_start", "pilot_yaw_right_174")
        require(180 <= yaw_start["sim_elapsed_s"]-ign["sim_elapsed_s"] < 180.2, "pilot yaw TIG-plus-three start required")
    elif target_enabled:
        actions = ("target_yaw_right_10", "target_yaw_right_174")
        require(all(e["action"] in actions and e["area_id"] == 0x51 for e in keyboard),
                "only declared original right-yaw target inputs allowed")
        for action, angle, row in zip(actions, (10, 174), (55, 169)):
            require(one("yaw_target_start", action)["checklist_row"] == row, "target yaw checklist row required")
            yaw_targets[action] = analyze_yaw_target(events, action, angle)
        yaw_start = one("yaw_target_start", actions[1])
        require(180 <= yaw_start["sim_elapsed_s"]-ign["sim_elapsed_s"] < 180.2,
                "target yaw must start at measured TIG-plus-three")
        require(not any(e["event"] in ("yaw_input_completed", "yaw_settling_sample") for e in events),
                "target driver must remain separate from exact-input regression")
    elif yaw_enabled:
        require([(e["event"], e["action"], e["area_id"]) for e in keyboard] ==
                [(kind, action, 0x51) for action in ("checklist_yaw_right_10", "checklist_yaw_right_174")
                 for kind in ("keyboard_down", "keyboard_up")], "only original NUMPAD3 checklist yaw inputs required")
        for action, angle, row in (("checklist_yaw_right_10", 10, 55), ("checklist_yaw_right_174", 174, 169)):
            down, up, done = (one(kind, action) for kind in ("keyboard_down", "keyboard_up", "yaw_input_completed"))
            require(down["seq"] < up["seq"] < done["seq"] and done["checklist_row"] == row,
                    "original yaw input/release/readback ordering required")
            require(angle <= done["yaw_integrated_deg"] < angle + .5 and not done["keyboard_yaw_right_down"]
                    and done["aca_keyboard_yaw_right"] == 0, "measured yaw input and neutral ACA readback required")
            yaw_settling[action] = analyze_yaw_settling(events, action, angle)
        yaw_start = one("keyboard_down", "checklist_yaw_right_174")
        require(180 <= yaw_start["sim_elapsed_s"] - ign["sim_elapsed_s"] < 180.2, "worksheet yaw must start at measured TIG-plus-three")
    else:
        require(not keyboard and not any(e["event"] in ("yaw_input_completed", "yaw_settling_sample", "yaw_target_start", "yaw_target_sample", "yaw_target_acquired") for e in events), "no hidden pilot yaw in observational scope")
    require(pilot_yaw or not any(e["event"].startswith("pilot_") for e in events), "hidden pilot input")
    geometry = last["radar_geometry"]
    require(math.isfinite(geometry["beam_nadir_cosine"]) and -1 <= geometry["beam_nadir_cosine"] <= 1,
            "finite beam/nadir cosine required")
    require(geometry["radar_test_switch"] == 1, "original operate mode required")
    require(geometry["range_good_discrete"] == (last["radar"]["RANGEGOOD"] == "1") and
            geometry["velocity_good_discrete"] == (last["radar"]["VELOCITYGOOD"] == "1"),
            "original radar flags and LGC data-good readbacks must agree")
    return {"checkpoint_reached": True, "actual_ignition_elapsed_s": ign["sim_elapsed_s"],
            "actual_ignition_minus_initial_lgc_tig_s": ign["sim_elapsed_s"] - events[0]["sim_elapsed_s"] - events[0]["countdown_s"],
            "lgc_countdown_at_dps_start_s": ign["countdown_s"],
            "n62_displayed_tfi_s": displayed_tfi,
            "n68_request_after_actual_ignition_s": request["sim_elapsed_s"] - ign["sim_elapsed_s"],
            "checkpoint_elapsed_s": last["sim_elapsed_s"], "observed_dh_ft": dh,
            "radar_valid": valid, "radar_measurement_available": valid and dh is not None and abs(dh) < 99999,
            "checkpoint_radar_geometry": geometry,
            "radar_range_unit": "ft", "radar_rate_unit": "ft/s", "n68_dh_unit": "ft",
            "operational_dh_envelope": None, "v57_sent": False, "full_checklist_fidelity": False,
            "pilot_driver_enabled":pilot_yaw,"pilot_body_yaw_targets":pilot_targets,
            "manual_yaw_measurement": ("Integral of original pilot body yaw including neutral braking; windows-up geometry observed" if pilot_yaw else
                                       "Relative quaternion yaw twist about input-start body Y; diagnostic angle/rate dwell only" if target_enabled else
                                       "Integral of -body-Y angular velocity while original NUMPAD3 is held; settled attitude not certified"),
            "manual_yaw_settled_attitude_validated": False,
            "manual_yaw_release_and_settling": yaw_settling,
            "yaw_target_driver_enabled": target_enabled, "diagnostic_relative_yaw_targets": yaw_targets,
            "manual_yaw_release_readback_delay_s": None if target_enabled or pilot_yaw else .2,
            "manual_yaw_after_release_rate_deg_s": {action: -one("yaw_input_completed", action)["angular_velocity_body_rad_s"][1] * 180 / math.pi
                for action in ("checklist_yaw_right_10", "checklist_yaw_right_174")} if yaw_enabled and not target_enabled and not pilot_yaw else {},
            "manual_yaw_10_exercised": yaw_enabled, "manual_yaw_174_exercised": yaw_enabled, "landing_attempted": False, "windows_comparison": False}


def validate_requested_scope(report, checklist_yaw=False, yaw_target=False):
    """A completed exact-input run cannot satisfy a requested target driver."""
    require(report["yaw_target_driver_enabled"] == yaw_target,
            "requested yaw target variant must match logged variant")
    require(report["manual_yaw_174_exercised"] == (checklist_yaw or yaw_target),
            "requested yaw scope must match logged inputs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path); parser.add_argument("orbiter", type=Path)
    parser.add_argument("runtime", type=Path); parser.add_argument("source", type=Path)
    parser.add_argument("--preflight-only", action="store_true")
    yaw_options = parser.add_mutually_exclusive_group()
    yaw_options.add_argument("--yaw-target", action="store_true", help="Separate braking-aware relative-axis diagnostic yaw driver")
    yaw_options.add_argument("--checklist-yaw", action="store_true", help="Bounded source-derived original NUMPAD3 yaw experiment")
    args = parser.parse_args()
    probe, orbiter, runtime, source = (p.resolve() for p in (args.probe, args.orbiter, args.runtime, args.source))
    report = preflight(source, runtime)
    if args.preflight_only:
        print(json.dumps({**report, "preflight_only": True, "simulator_run": False}, indent=2)); return
    started = time.monotonic()
    run = subprocess.run([str(probe), str(runtime), "1500"] + (["--yaw-target"] if args.yaw_target else ["--checklist-yaw"] if args.checklist_yaw else []), text=True, capture_output=True, timeout=360)
    (runtime / "PDIProtocol.log").write_text(run.stdout + run.stderr)
    event_log = runtime / "PDIProtocol.jsonl"
    events = [json.loads(line) for line in event_log.read_text().splitlines()] if event_log.exists() else []
    if run.returncode:
        (runtime / "pdi-protocol-verification.json").write_text(json.dumps({**report, "checkpoint_reached": False,
            "returncode": run.returncode, "last_event": events[-1] if events else None}, indent=2) + "\n")
    require(run.returncode == 0, "bounded protocol failed; inspect PDIProtocol.jsonl and PDIProtocol.log")
    try:
        report.update(validate_events(events))
        validate_requested_scope(report, args.checklist_yaw, args.yaw_target)
    except ValueError as error:
        (runtime / "pdi-protocol-verification.json").write_text(json.dumps({**report,
            "checkpoint_reached": False, "verification_error": str(error),
            "last_event": events[-1] if events else None}, indent=2) + "\n")
        raise
    report["binary_sha256"] = {name: hashlib.sha256(path.read_bytes()).hexdigest()
                               for name, path in (("probe", probe), ("orbiter", orbiter))}
    original = vessels(source.read_text())
    saved = vessels((runtime / "Scenarios/NativePDIProtocolCheckpoint.scn").read_text())
    require({n: s[0] for n, s in original.items()} == {n: s[0] for n, s in saved.items()}, "all original vessels must survive")
    reload = subprocess.run([str(orbiter), "--root", str(runtime), "--scenario", "NativePDIProtocolCheckpoint",
                             "--steps", "0", "--save", "NativePDIProtocolReloaded"],
                            text=True, capture_output=True, timeout=120)
    (runtime / "PDIProtocolReload.log").write_text(reload.stdout + reload.stderr)
    require(reload.returncode == 0, "fresh-process zero-step checkpoint reload failed")
    again = vessels((runtime / "Scenarios/NativePDIProtocolReloaded.scn").read_text())
    require({n: s[0] for n, s in again.items()} == {n: s[0] for n, s in saved.items()}, "fresh reload must retain vessel classes")
    require(agc(again["Eagle"][1]) == agc(saved["Eagle"][1]), "fresh reload must retain LGC memory/channels")
    require(field(again["Eagle"][1], "MISSNTIME") == field(saved["Eagle"][1], "MISSNTIME"), "zero-step reload must retain mission clock")
    require(field(again["Eagle"][1], "NOUN") == "68", "fresh reload must retain N68")
    require("ERROR:" not in (runtime / "Orbiter.log").read_text(errors="replace"), "clean reload log required")
    require((runtime / "Scenarios" / (SCENARIO + ".scn")).read_bytes() == source.read_bytes(), "supplied scenario remains unchanged")
    report.update({"fresh_zero_step_reload": True, "lgc_memory_channels_retained": True,
                   "wall_seconds": time.monotonic() - started,
                   "event_log_sha256": hashlib.sha256((runtime / "PDIProtocol.jsonl").read_bytes()).hexdigest(),
                   "limits": ["Prior power setup, visual checks, AGS updates, communications and full pilot fidelity not validated",
                              "Radar validity and dH are observations; no operational envelope or V57 approval encoded",
                              "Original callback inputs do not validate graphical hit testing or human piloting"]})
    (runtime / "pdi-protocol-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
