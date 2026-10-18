"""Separate original-input pilot procedure through the 2000-ft manual handoff.

The published crew-chart plateau is bounded to 27,500..40,700 ft. Yaw angle
and neutral-rate tolerances are test observers, not Apollo flight tolerances.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import subprocess
import time

from NativePDIProtocol import (SCENARIO, KEY_INPUTS, N68_KEYS, require,
                               validate_events, preflight, vessels, agc, field)

from NativePDIPersistence import verify_persistence, continuation_comparison, processor_persistence

V57_KEYS = [3401, 3410, 3412, 3418]
TAIL_KEYS = {
    "enable_lr_v57": V57_KEYS,
    "post_v57_n68": N68_KEYS,
    "return_n63": [3401, 3406, 3411, 3402, 3411, 3408, 3418],
    "monitor_n68": N68_KEYS,
    "approach_key_rel": [3417],
    "approach_pro": [3416],
}
SOURCE_CHART = {
    "altitude_interval_ft": [27500, 40700], "maximum_absolute_dh_ft": 17500,
    "rule": "Apollo 11 mission rule 5-89, printed 5-13 (1969-04-16)",
    "rule_url": "https://www.nasa.gov/wp-content/uploads/static/history/alsj/a11/a11missionrules.pdf",
    "crew_chart": "LM crew chart 9, printed 50 (1969-06-27)",
    "crew_chart_url": "https://www.ibiblio.org/apollo/Documents/Apollo%2011%20Crew%20Charts%20And%20Console%20Charts.pdf",
    "timeline": "Final flown LM timeline, printed 9, revision G (1969-07-01)",
    "timeline_url": "https://www.ibiblio.org/apollo/Documents/Apollo11-LM-TimelineBook-excerpts.pdf",
    "qualification": "MTM 28D item 2 (1969-07-09): MIT had not independently verified this published envelope",
    "qualification_url": "https://www.ibiblio.org/apollo/Documents/mission_techniques_memo_28d.pdf",
    "extrapolation": False,
}


def dh(event):
    value = event["dsky"]["R3"]
    require(re.fullmatch(r"[+-][0-9]{5}", value) is not None,
            "complete signed N68 dH required")
    require(abs(int(value)) < 99999, "N68 sentinel is not a measurement")
    return int(value)


def radar_acceptance(event):
    require((event["dsky"]["PROG"], event["dsky"]["VERB"], event["dsky"]["NOUN"])
            == ("63", "16", "68"), "P63 V16N68 decision required")
    h = event["lgc_altitude_ft"]
    require(isinstance(h, (int, float)) and math.isfinite(h) and 27500 <= h <= 40700,
            "altitude outside published crew-chart interval")
    value = dh(event)
    require(abs(value) <= 17500, "dH exceeds published crew-chart limit")
    radar, geometry = event["radar"], event["radar_geometry"]
    require(radar["RANGEGOOD"] == "1" and radar["VELOCITYGOOD"] == "1" and
            geometry["range_good_discrete"] and geometry["velocity_good_discrete"],
            "original radar and LGC data-good agreement required")
    require(geometry["radar_test_switch"] == 1 and geometry["position1_discrete"] and
            not geometry["position2_discrete"], "original descent radar operating position required")
    require(math.isfinite(float(radar["RANGE"])) and float(radar["RANGE"]) > 0,
            "positive finite radar range required")
    require(not int(event["lgc_flagword11_octal"], 8) & 0o200,
            "original LR incorporation must be inhibited before V57")
    return {"lgc_altitude_ft": h, "dh_ft": value, "source_chart": SOURCE_CHART}


def audit_keys(events, expected):
    stream = [(action, area) for action, areas in expected.items() for area in areas]
    down = [e for e in events if e["event"] == "input_down" and 3400 <= e["area_id"] <= 3418]
    up = [e for e in events if e["event"] == "input_up" and 3400 <= e["area_id"] <= 3418]
    for observed in (down, up):
        require([(e["action"], e["area_id"]) for e in observed] == stream,
                "complete pilot DSKY command stream required")
    for d, u in zip(down, up):
        require(d["seq"] < u["seq"] and abs(u["sim_elapsed_s"] - d["sim_elapsed_s"] -
                (2 if d["area_id"] == 3416 else .2)) < .02,
                "paired measured original DSKY hold required")


def validate_pilot_events(events):
    require(events and events[-1]["event"] == "checkpoint" and events[-1]["action"] ==
            "pilot_neutral_reference_complete", "pilot handoff and neutral reference checkpoint required")
    require(not any(e["event"] == "blocked" for e in events), "pilot procedure was blocked")
    first = [e for e in events if e["event"] == "checkpoint" and e["action"] ==
             "v57_decision_pending_no_operational_dh_envelope"]
    require(len(first) == 1, "one original first-N68 prefix checkpoint required")
    prefix = events[:first[0]["seq"] + 1]
    report = validate_events(prefix, pilot_yaw=True)
    tail = events[len(prefix):]
    previous, offset = -1, events[0]["mission_time_s"]
    for seq, e in enumerate(events):
        require(e["seq"] == seq and e["pilot_driver_enabled"] and e["pilot_descent_enabled"] and
                e["checklist_yaw_enabled"] and not e["yaw_target_driver_enabled"],
                "consistent separate pilot-descent scope and event order required")
        require(math.isfinite(e["sim_elapsed_s"]) and previous <= e["sim_elapsed_s"] <= 1800 and
                abs(e["mission_time_s"] - e["sim_elapsed_s"] - offset) < .01,
                "bounded original simulation and mission clock required")
        require(not e["ground_contact"], "manual handoff cannot claim a landing")
        previous = e["sim_elapsed_s"]
    require(not any(e["event"] in ("keyboard_down", "keyboard_up", "pilot_axis_input") for e in tail),
            "unexpected post-yaw pilot input")
    require(all(e["pilot_aca_axis_s16"] == 0 and not e["aca_out_of_detent"] and
                e["aca_proportional_roll_pitch_yaw"] == [0, 0, 0] for e in tail),
            "neutral original ACA required during guided descent")
    audit_keys(events, {**KEY_INPUTS, **TAIL_KEYS})

    def one(kind, action):
        found = [e for e in events if e["event"] == kind and e["action"] == action]
        require(len(found) == 1, "exactly one pilot " + action + " required")
        return found[0]

    decision = one("radar_decision", "source_chart_17500ft_27500_to40700ft")
    acceptance = radar_acceptance(decision)
    enabled = one("radar_update_enabled", "original_luminary_lrinh_flag_set")
    monitor = one("display_confirmed", "post_v57_n68")
    converged = one("radar_convergence", "n68_dh_decreasing_after_v57")
    v57 = [e for e in tail if e["event"] == "input_down" and e["action"] == "enable_lr_v57"]
    require(first[0]["seq"] < decision["seq"] < v57[0]["seq"] < v57[-1]["seq"] <
            enabled["seq"] < monitor["seq"] < converged["seq"], "source decision/V57/N68 observation order required")
    require(int(enabled["lgc_flagword11_octal"], 8) & 0o200 and
            int(converged["lgc_flagword11_octal"], 8) & 0o200, "original V57 LRINH flag must permit updates")
    require((monitor["dsky"]["VERB"], monitor["dsky"]["NOUN"]) == ("16", "68") and
            (converged["dsky"]["VERB"], converged["dsky"]["NOUN"]) == ("16", "68") and
            abs(converged["sim_elapsed_s"] - monitor["sim_elapsed_s"] - 30) < .02,
            "reselected N68 and bounded 30-second convergence observation required")
    require(abs(dh(converged)) < abs(acceptance["dh_ft"]), "observed N68 dH must decrease after V57")

    controls = [e for e in tail if e["event"] == "input_down" and not 3400 <= e["area_id"] <= 3418]
    require([(e["action"], e["area_id"]) for e in controls] ==
            [("ModeControlPGNSSwitch", 3312), ("ModeControlPGNSSwitch", 3312),
             ("LandingAntSwitch", 3303), ("LandingAntSwitch", 3303)],
            "complete pilot control stream required")
    releases = [e for e in tail if e["event"] == "input_up" and not 3400 <= e["area_id"] <= 3418]
    require([(e["action"], e["area_id"]) for e in releases] == [(e["action"], e["area_id"]) for e in controls],
            "every original pilot control must be released")
    for down, up in zip(controls, releases):
        require(down["seq"] < up["seq"] and abs(up["sim_elapsed_s"] - down["sim_elapsed_s"] - .2) < .02,
                "paired measured pilot control hold required")
    confirmations = [e for e in tail if e["event"] == "state_confirmed"]
    require([(e["action"], e["requested_state"], e["readback"]) for e in confirmations] ==
            [("ModeControlPGNSSwitch", 1, 1), ("ModeControlPGNSSwitch", 2, 2), ("LandingAntSwitch", 0, 0)],
            "original pilot control readbacks required")
    p64 = one("program_transition", "original_p64")
    n64 = one("display_confirmed", "approach_n64")
    pro = one("input_down", "approach_pro")
    last = one("checkpoint", "pilot_guided_descent_2000ft_manual_handoff_pending")
    require(p64["dsky"]["PROG"] == "64" and p64["seq"] < controls[-2]["seq"] < controls[-1]["seq"] < n64["seq"] < pro["seq"],
            "original P64 must precede HOVER, N64 and PRO")
    require((n64["dsky"]["PROG"], n64["dsky"]["VERB"], n64["dsky"]["NOUN"]) == ("64", "06", "64"),
            "original P64 N64 display required")
    ignition = report["actual_ignition_elapsed_s"]
    for action, seconds in (("tig_plus_6m40s", 400), ("tig_plus_8_minutes", 480), ("tig_plus_9_minutes", 540)):
        timing = one("timing", action)
        require(seconds <= timing["sim_elapsed_s"] - ignition < seconds + .2,
                "pilot timeline must use measured DPS ignition: " + action)
    require(540 <= pro["sim_elapsed_s"] - ignition < 541 and
            last["dsky"]["PROG"] in ("64", "65") and 0 < last["lgc_altitude_ft"] <= 2000 and
            last["dps_level"] > 0 and last["controls"]["LandingAntSwitch"] == 0,
            "original guided descent must reach the bounded 2000-ft manual handoff")
    one("deferred", "manual_control_evaluation_requires_pilot_judgment")
    one("deferred", "ags_deda_223_enter_and_manual_landing_not_validated")
    report.update({"pilot_descent_enabled": True, "v57_sent": True,
                   "operational_dh_envelope": SOURCE_CHART, "radar_acceptance": acceptance,
                   "post_v57_dh_ft": dh(converged), "convergence_observation_seconds": 30,
                   "radar_adequacy_for_entire_mission_certified": False,
                   "original_p64_elapsed_s": p64["sim_elapsed_s"],
                   "checkpoint_elapsed_s": last["sim_elapsed_s"],
                   "handoff_lgc_altitude_ft": last["lgc_altitude_ft"],
                   "handoff_guidance_program": last["dsky"]["PROG"],
                   "manual_landing_validated": False, "ags_deda_validated": False,
                   "graphical_hit_testing_validated": False})
    return report


def verify_reload(probe, orbiter, runtime, source, events):
    original = vessels(source.read_text())
    saved = vessels((runtime / "Scenarios/NativePDIPilotHandoff.scn").read_text())
    require({n: s[0] for n, s in original.items()} == {n: s[0] for n, s in saved.items()},
            "all original vessels must survive the pilot procedure")
    run = subprocess.run([str(orbiter), "--root", str(runtime), "--scenario", "NativePDIPilotHandoff",
                          "--steps", "0", "--save", "NativePDIPilotReloaded"],
                         text=True, capture_output=True, timeout=120)
    (runtime / "PDIPilotReload.log").write_text(run.stdout + run.stderr)
    require(run.returncode == 0, "fresh-process zero-step pilot reload required")
    again = vessels((runtime / "Scenarios/NativePDIPilotReloaded.scn").read_text())
    require({n: s[0] for n, s in saved.items()} == {n: s[0] for n, s in again.items()} and
            agc(saved["Eagle"][1]) == agc(again["Eagle"][1]) and
            field(saved["Eagle"][1], "MISSNTIME") == field(again["Eagle"][1], "MISSNTIME"),
            "reload must retain all vessel classes, original LGC memory/channels and clock")
    exact = verify_persistence((runtime / "Scenarios/NativePDIPilotHandoff.scn").read_text(),
                       (runtime / "Scenarios/NativePDIPilotReloaded.scn").read_text())
    require("ERROR:" not in (runtime / "Orbiter.log").read_text(errors="replace"), "clean pilot reload log required")
    neutral = subprocess.run([str(probe), str(runtime), "1200", "--resume-neutral"],
                             text=True, capture_output=True, timeout=120)
    (runtime / "PDIPilotResume.log").write_text(neutral.stdout + neutral.stderr)
    require(neutral.returncode == 0, "fresh-process neutral continuation probe required")
    resumed = [json.loads(line) for line in (runtime / "PDIPilotResume.jsonl").read_text().splitlines()]
    processor = processor_persistence((runtime / "Scenarios/NativePDIPilotHandoff.scn").read_text(),
                                      (runtime / "Scenarios/NativePDIPilotReloaded.scn").read_text())
    observation = continuation_comparison(events, resumed)
    return {"zero_step_persistence": exact, "processor_resume": processor,
            "neutral_continuation": observation,
            "faithful_neutral_resume": processor["cycle_counter_retained"] and
                observation["continuation_matches_observation_bounds"],
            "aca_pulse_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("probe", "orbiter", "runtime", "source"):
        parser.add_argument(arg, type=Path)
    parser.add_argument("--verify-existing", action="store_true", help="Verify an already completed run; do not repeat simulation")
    args = parser.parse_args()
    probe, orbiter, runtime, source = (getattr(args, k).resolve() for k in ("probe", "orbiter", "runtime", "source"))
    original_inputs = preflight(source, runtime)
    report = dict(original_inputs)
    started = time.monotonic()
    try:
        if not args.verify_existing:
            run = subprocess.run([str(probe), str(runtime), "1800", "--pilot-descent"],
                                 text=True, capture_output=True, timeout=360)
            (runtime / "PDIPilot.log").write_text(run.stdout + run.stderr)
            require(run.returncode == 0, "pilot probe failed; inspect PDIProtocol.jsonl and PDIPilot.log")
        events = [json.loads(line) for line in (runtime / "PDIProtocol.jsonl").read_text().splitlines()]
        report.update(validate_pilot_events(events))
        report.update(verify_reload(probe, orbiter, runtime, source, events))
        report.update({"fresh_zero_step_reload": True, "lgc_memory_channels_retained": True})
        require(preflight(source, runtime) == original_inputs, "supplied assets must remain unchanged")
        require(not (runtime / "Config/ProjectApollo/Vesim/LM.native.cfg").exists(), "pilot fixture must clean its controller binding")
        report.update({"fresh_zero_step_reload": True, "lgc_memory_channels_retained": True,
                       "verification_wall_seconds": time.monotonic() - started,
                       "binary_sha256": {k: hashlib.sha256(p.read_bytes()).hexdigest() for k, p in
                                         (("probe", probe), ("orbiter", orbiter))},
                       "event_log_sha256": hashlib.sha256((runtime / "PDIProtocol.jsonl").read_bytes()).hexdigest(),
                       "limits": ["Observer yaw tolerances are not Apollo flight tolerances",
                                  "Crew-chart envelope applies only inside the cited altitude interval",
                                  "30-second net convergence does not certify all radar or mission go/no-go criteria",
                                  "Prior visual checks, communications, AGS DEDA, manual evaluation and landing remain unvalidated",
                                  "Original callback inputs do not validate graphical hit testing or full human piloting"]})
        require(report["processor_resume"]["cycle_counter_retained"],
                "AGC cycle counter was not restored; faithful processor resume requires CDU FIFO persistence")
        require(report["neutral_continuation"]["continuation_matches_observation_bounds"],
                "neutral handoff continuation differs beyond scenario precision bounds; inspect neutral_continuation evidence")
        report["validation_passed"] = True
    except (ValueError, subprocess.TimeoutExpired) as error:
        report.update({"checkpoint_reached": report.get("checkpoint_reached", False),
                       "validation_passed": False, "verification_error": str(error)})
        (runtime / "pdi-pilot-verification.json").write_text(json.dumps(report, indent=2) + "\n")
        raise
    (runtime / "pdi-pilot-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
