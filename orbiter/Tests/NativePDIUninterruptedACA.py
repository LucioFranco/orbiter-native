"""Bounded original ACA pulse/release after P64 in one uninterrupted process.

Observer bounds are diagnostic, not Apollo flight tolerances. This does not
certify saved-state resume, physical controls, landing or landing-site judgment.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import subprocess
import time

from NativePDIProtocol import require, preflight, vessels
from NativePDIPilot import validate_pilot_events

PHASES = (("neutral_baseline", 60), ("yaw_pulse", 18), ("neutral_release", 240))
ACA_YAW_FRACTION = (-14823 * 26.0 / 65536 + 1.0) / 10.0  # Original lm_aca.cpp detent/scaling.
CONTROLS = {"ModeControlPGNSSwitch": 1, "RollSwitch": 2, "PitchSwitch": 2,
            "YawSwitch": 2, "RateScaleSwitch": 1, "DeadBandSwitch": 0,
            "THRContSwitch": 1, "ATTTranslSwitch": 1, "BALCPLSwitch": 1}


def rate(e):
    return -math.degrees(e["angular_velocity_body_rad_s"][1])


def measure_aca_samples(samples):
    require(len(samples) == sum(n + 1 for _, n in PHASES), "complete 321-frame ACA observation required")
    by_phase, offset, previous_cycle = {}, 0, -1
    for name, frames in PHASES:
        phase = samples[offset:offset + frames + 1]
        offset += frames + 1
        by_phase[name] = phase
        for tick, e in enumerate(phase):
            require(e["event"] == "handoff_aca_sample" and e["action"] == name and e["aca_phase_tick"] == tick,
                    "ordered ACA phase samples required")
            require(e["handoff_aca_enabled"] and e["pilot_driver_enabled"] and e["pilot_descent_enabled"] and
                    not e["yaw_target_driver_enabled"], "original uninterrupted input scope required")
            require(abs(e["sim_elapsed_s"] - phase[0]["sim_elapsed_s"] - tick/60) < 1e-5,
                    "measured fixed-duration ACA phase required")
            require(abs(e["mission_time_s"] - phase[0]["mission_time_s"] - tick/60) < 1e-5,
                    "continuous mission clock required")
            require(e["lgc_cycle_counter"] > 1000000 and e["lgc_cycle_counter"] >= previous_cycle,
                    "uninterrupted LGC cycle history required")
            previous_cycle = e["lgc_cycle_counter"]
            require(not e["ground_contact"] and e["lgc_altitude_ft"] > 1200 and e["dps_level"] > 0 and
                    e["dsky"]["PROG"] in ("64", "65"), "bounded original guided descent required")
            require(all(e["controls"][k] == v for k, v in CONTROLS.items()), "original ATT HOLD control readbacks required")
            require(e["guidance_channel_031"] & (1 << 12) and not e["guidance_channel_031"] & (1 << 13),
                    "original ATT HOLD guidance discrete required")
            require(not e["guidance_relay_k12"] and not e["guidance_relay_k13"] and e["gasta_powered"],
                    "powered original PGNS control and attitude observer required")
            require(len(e["rcs_jet_levels"]) == 16 and all(0 <= v <= 1 for v in e["rcs_jet_levels"]),
                    "all original RCS levels required")
            for key in ("angular_velocity_body_rad_s", "rcs_torque_body_nm", "pgns_fdai_rpy_deg"):
                require(len(e[key]) == 3 and all(math.isfinite(v) for v in e[key]), "finite original motion and torque required")
            r = e["rotation_body_to_global"]
            require(len(r) == 3 and all(len(row) == 3 and all(math.isfinite(v) for v in row) for row in r),
                    "finite original attitude matrix required")
            for i in range(3):
                for j in range(3):
                    require(abs(sum(r[k][i]*r[k][j] for k in range(3)) - (i == j)) < 1e-8,
                            "original attitude matrix must remain orthonormal")
            axis = 14823 if name == "yaw_pulse" else 0
            require(e["pilot_aca_axis_s16"] == axis, "fixed original axis command required")
            if tick:  # Input-event and phase-zero readbacks precede the original systems scan.
                expected = [0, 0, ACA_YAW_FRACTION] if axis else [0, 0, 0]
                require(all(abs(x-y) <= 1e-12 for x,y in zip(e["aca_proportional_roll_pitch_yaw"], expected)) and
                        len(e["aca_proportional_roll_pitch_yaw"]) == 3 and e["aca_out_of_detent"] == bool(axis),
                        "original ACA command and neutral-release readbacks required")
                require(bool(e["guidance_channel_031"] & (1 << 14)) == bool(axis),
                        "original out-of-detent guidance discrete required")
                pair = (e["lgc_rhc_yaw_raw_s32"], e["lgc_rhc_yaw_count"])
                require(pair in ((0,0),(-20,-19)), "original signed RHC counter required")
                if tick >= 12:  # Counter reads are requested by the original 100ms DAP, not each input frame.
                    require(pair == ((-20,-19) if axis else (0,0)), "bounded original RHC read/release latency required")
        if name != "neutral_baseline":
            prior = by_phase[PHASES[list(by_phase).index(name)-1][0]][-1]
            require(abs(phase[0]["sim_elapsed_s"] - prior["sim_elapsed_s"]) < 1e-5 and
                    phase[0]["lgc_cycle_counter"] == prior["lgc_cycle_counter"], "same live phase boundary required")
    baseline, pulse, release = (by_phase[n] for n, _ in PHASES)
    response = pulse[1:] + release[1:7]
    require(any(e["rcs_torque_body_nm"][1] > 1 and rate(e) > .2 for e in release[1:61]),
            "original opposing braking torque after neutral release required")
    require(any(any(e["pgns_jet_requests"]) for e in response) and
            any(any(e["jet_driver"]) for e in response) and
            any(any(e["rcs_jet_levels"]) for e in response) and
            any(e["rcs_torque_body_nm"][1] < -1 for e in response), "original right-yaw command-to-jet response required")
    baseline_max = max(abs(rate(e)) for e in baseline)
    peak = max(rate(e) for e in pulse + release)
    require(peak > baseline_max + .2, "measurable right-yaw response above neutral baseline required")
    final_max = max(abs(rate(e)) for e in release[-61:])
    require(final_max <= .15 and peak > final_max + .2, "bounded original neutral damping and one-second quiet dwell required")
    integrated = sum((rate(a)+rate(b))/120 for a, b in zip(pulse, pulse[1:]))
    integrated += sum((rate(a)+rate(b))/120 for a, b in zip(release, release[1:]))
    require(integrated > .03, "measured original body-yaw motion required")
    matrix_change = max(abs(pulse[0]["rotation_body_to_global"][i][j] - release[-1]["rotation_body_to_global"][i][j])
                        for i in range(3) for j in range(3))
    require(matrix_change > 1e-4, "measured attitude response required")
    return {"same_live_simulation": True, "input_path": "SDL virtual stick -> native VESIM -> original ACA/LGC/DAP/ATCA -> RCS",
            "pulse_axis_s16": 14823, "raw_rhc_count": -20, "decoded_rhc_count": -19, "aca_proportional_yaw":ACA_YAW_FRACTION,
            "neutral_baseline_seconds": 1, "pulse_seconds": .3, "neutral_release_seconds": 4,
            "samples": len(samples), "baseline_max_abs_yaw_rate_deg_s": baseline_max,
            "peak_right_yaw_rate_deg_s": peak, "final_yaw_rate_deg_s": rate(release[-1]),
            "last_second_max_abs_yaw_rate_deg_s": final_max, "pulse_and_release_body_yaw_deg": integrated,
            "attitude_matrix_max_change": matrix_change,
            "first_response_delay_seconds": min(e["sim_elapsed_s"]-pulse[0]["sim_elapsed_s"] for e in response if e["rcs_torque_body_nm"][1] < -1),
            "minimum_lgc_altitude_ft": min(e["lgc_altitude_ft"] for e in samples),
            "programs_observed": sorted({e["dsky"]["PROG"] for e in samples}),
            "observer_limits": {"quiet_yaw_rate_deg_s": .15, "quiet_dwell_seconds": 1, "minimum_altitude_ft":1200},
            "final_sample":release[-1], "manual_landing_validated":False, "physical_controller_validated":False,
            "faithful_resume_validated":False}


def validate_aca_events(events):
    ends = [e for e in events if e["event"] == "checkpoint" and e["action"] == "pilot_neutral_reference_complete"]
    require(len(ends) == 1, "one complete uninterrupted PDI prefix required")
    prefix_end = ends[0]["seq"]
    report = validate_pilot_events(events[:prefix_end+1])
    require(events[-1]["event"] == "checkpoint" and events[-1]["action"] == "uninterrupted_aca_pulse_release_complete",
            "complete uninterrupted ACA checkpoint required")
    require(all(e["seq"] == i and e["handoff_aca_enabled"] for i, e in enumerate(events)) and
            not any(e["event"] == "blocked" for e in events), "uninterrupted original event scope and order required")
    tail = events[prefix_end+1:]
    downs = [e for e in tail if e["event"] == "input_down"]
    ups = [e for e in tail if e["event"] == "input_up"]
    confirmed = [e for e in tail if e["event"] == "state_confirmed"]
    require(len(downs) == len(ups) == len(confirmed) == 1 and
            downs[0]["action"] == ups[0]["action"] == confirmed[0]["action"] == "ModeControlPGNSSwitch" and
            downs[0]["area_id"] == 3312 and downs[0]["seq"] < ups[0]["seq"] < confirmed[0]["seq"] and
            confirmed[0]["requested_state"] == confirmed[0]["readback"] == 1,
            "one original switch callback to ATT HOLD required")
    inputs = [e for e in tail if e["event"] == "handoff_aca_input"]
    require([(e["action"], e["pilot_aca_axis_s16"]) for e in inputs] == [("yaw_right",14823),("neutral_release",0)],
            "exact fixed pulse and neutral-release input stream required")
    require(not any(e["event"] in ("keyboard_down","keyboard_up","pilot_axis_input") for e in tail),
            "no additional handoff control input permitted")
    report["uninterrupted_aca"] = measure_aca_samples([e for e in tail if e["event"] == "handoff_aca_sample"])
    report.update({"validation_scope":"Uninterrupted original PDI/P64 plus bounded automated ACA diagnostic; no snapshot resume", "aca_pulse_executed":True})
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("probe","runtime","source"):parser.add_argument(name,type=Path)
    parser.add_argument("--verify-existing",action="store_true")
    args=parser.parse_args();probe,runtime,source=(getattr(args,k).resolve() for k in ("probe","runtime","source"))
    initial=preflight(source,runtime);report=dict(initial);started=time.monotonic()
    try:
        if not args.verify_existing:
            run=subprocess.run([str(probe),str(runtime),"1800","--pilot-aca"],text=True,capture_output=True,timeout=600)
            (runtime/'PDIUninterruptedACA.log').write_text(run.stdout+run.stderr)
            require(run.returncode==0,"uninterrupted ACA probe failed; inspect PDIProtocol.jsonl")
        events=[json.loads(l) for l in (runtime/'PDIProtocol.jsonl').read_text().splitlines()]
        report.update(validate_aca_events(events))
        saved=vessels((runtime/'Scenarios/NativePDIPilotHandoff.scn').read_text())
        require({n:s[0] for n,s in saved.items()}=={n:s[0] for n,s in vessels(source.read_text()).items()},"all original handoff vessels required")
        require(preflight(source,runtime)==initial,"supplied scenario, ROM, configuration and workbook must remain unchanged")
        require(not (runtime/'Config/ProjectApollo/Vesim/LM.native.cfg').exists(),"temporary ACA binding must be removed")
        require('ERROR:' not in (runtime/'Orbiter.log').read_text(errors='replace'),"clean native simulation log required")
        report.update({"validation_passed":True,"verification_wall_seconds":time.monotonic()-started,
                       "probe_sha256":hashlib.sha256(probe.read_bytes()).hexdigest(),
                       "event_log_sha256":hashlib.sha256((runtime/'PDIProtocol.jsonl').read_bytes()).hexdigest()})
    except (ValueError,subprocess.TimeoutExpired) as e:
        report.update({"validation_passed":False,"verification_error":str(e)})
        (runtime/'pdi-uninterrupted-aca-verification.json').write_text(json.dumps(report,indent=2)+'\n')
        raise
    (runtime/'pdi-uninterrupted-aca-verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
