"""Log-contract regression tests using synthetic observations, never flight proof."""
import unittest
from NativePDIProtocol import agc_seconds, validate_events, FINAL_CONTROLS, N68_KEYS, analyze_yaw_settling


def words(seconds):
    high, low = divmod(round(seconds * 100), 16384)
    return format(high if high >= 0 else 0o77777 + high, "o"), format(low, "o")


def observations(ignition_elapsed=901.15):
    """Independent chronological oracle for the bounded protocol's log contract."""
    events = []
    controls = {**FINAL_CONTROLS, "ModeControlPGNSSwitch": 1, "ModeControlAGSSwitch": 1,
                "EngineArmSwitch": 1, "EngineDescentCommandOverrideSwitch": 0,
                "PGNS_LDG_RDR_CB": 0, "RateScaleSwitch": 0, "AltRngMonSwitch": 1,
                "RendezvousRadarRotary": 0, "ORDEALFDAI2Switch": 1}
    dsky = {"PROG": "63", "VERB": "50", "NOUN": "18", "R1": "+17736",
            "R2": "+28638", "R3": "+35976", "STATE": "0"}
    tig = 901.15

    def add(t, kind, action, area=0, field="", target=-1, row=0):
        high, low = words(t - tig)
        events.append({"seq": len(events), "event": kind, "action": action,
                       "checklist_row": row, "checklist_yaw_enabled": False, "area_id": area, "mouse_event": 0,
                       "sim_elapsed_s": t, "mission_time_s": 368744.423737 + t,
                       "agc_clock_s": round(368743.87 + t, 2), "tig_s": 369645.02,
                       "countdown_s": round(tig - t, 2), "lgc_tfi_s": round(t - tig, 2),
                       "tfi_high_octal": high, "tfi_low_octal": low,
                       "actual_ignition_elapsed_s": ignition_elapsed if t >= ignition_elapsed else None,
                       "dps_level": .1 if t >= ignition_elapsed else 0,
                       "guidance_channel_011": 1 << 12 if t >= ignition_elapsed else 0,
                       "radar_geometry": {"beam_nadir_cosine": .9, "radar_test_switch": 1,
                                          "range_good_discrete": True, "velocity_good_discrete": True}, "ground_contact": False,
                       "requested_field": field, "requested_state": target,
                       "readback": controls[field] if field else None,
                       "dsky": dsky.copy(), "radar": {"RANGEGOOD": "1", "VELOCITYGOOD": "1"},
                       "controls": controls.copy()})

    def key(t, area, action):
        add(t, "input_down", action, area)
        add(t + (2 if area == 3416 else .2), "input_up", action, area)

    def control(t, field, target):
        controls[field] = target
        add(t, "state_confirmed", field, field=field, target=target)

    add(0, "initial", "original_scenario_loaded")
    control(10, "ModeControlPGNSSwitch", 2)
    key(10.4, 3416, "initial_auto_maneuver_pro")
    add(15, "display_confirmed", "initial_auto_maneuver_return")
    control(602, "PGNS_LDG_RDR_CB", 1)
    control(602.4, "ORDEALFDAI2Switch", 0)
    key(661.15, 3416, "final_trim_pro")
    add(670, "display_confirmed", "final_trim_return")
    key(670.1, 3418, "final_trim_enter")
    dsky.update(VERB="06", NOUN="62", R1="+55685", R2="-03 51", R3="+00000")
    add(670.5, "display_confirmed", "n62_countdown")
    control(671, "RateScaleSwitch", 1)
    control(671.4, "ModeControlAGSSwitch", 2)
    for i, area in enumerate([3401, 3412, 3412, 3418]):
        key(721.15 + i * .4, area, "v77")
    control(866.15, "EngineArmSwitch", 0)
    dsky.update(VERB="99")
    key(896.15, 3416, "ignition_pro")
    add(ignition_elapsed, "ignition", "first_positive_original_dps")
    dsky.update(VERB="06", NOUN="63")
    add(ignition_elapsed + .1, "display_confirmed", "ignition_n63")
    add(ignition_elapsed + .2, "input_down", "EngineDescentCommandOverrideSwitch", area=3302)
    controls["EngineDescentCommandOverrideSwitch"] = 1
    add(ignition_elapsed + .4, "input_up", "EngineDescentCommandOverrideSwitch", area=3302)
    control(ignition_elapsed + .6, "EngineDescentCommandOverrideSwitch", 1)
    add(ignition_elapsed + 26, "timing", "tig_plus_26_seconds")
    add(ignition_elapsed + 150, "timing", "tig_plus_2m30s")
    control(ignition_elapsed + 150.1, "AltRngMonSwitch", 0)
    control(ignition_elapsed + 150.5, "RendezvousRadarRotary", 1)
    add(ignition_elapsed + 180, "timing", "tig_plus_3_minutes")
    add(ignition_elapsed + 300, "timing", "tig_plus_5_minutes")
    add(ignition_elapsed + 300, "n68_request", "request_n68", row=177)
    for i, area in enumerate(N68_KEYS):
        key(ignition_elapsed + 300 + i * .4, area, "request_n68")
    dsky.update(VERB="16", NOUN="68", R3="+02800")
    add(ignition_elapsed + 302.8, "display_confirmed", "n68_display")
    add(ignition_elapsed + 307.8, "checkpoint", "v57_decision_pending_no_operational_dh_envelope", row=179)
    return events


class ProtocolContract(unittest.TestCase):
    def setUp(self):
        self.events = observations()

    def event(self, kind, action):
        return next(e for e in self.events if e["event"] == kind and e["action"] == action)

    def rejects(self, reason):
        with self.assertRaisesRegex(ValueError, reason):
            validate_events(self.events)

    def test_signed_agc_pairs_including_mixed_sign(self):
        for seconds in [0, -0.01, 901.15, -901.15, 368743.87, 369645.02]:
            high, low = words(seconds)
            self.assertAlmostEqual(agc_seconds(int(high, 8), int(low, 8)), seconds)

    def test_observational_checkpoint_does_not_approve_v57(self):
        report = validate_events(self.events)
        self.assertEqual(report["actual_ignition_elapsed_s"], 901.15)
        self.assertEqual(report["observed_dh_ft"], 2800)
        self.assertTrue(report["radar_measurement_available"])
        self.assertIsNone(report["operational_dh_envelope"])
        self.assertFalse(report["v57_sent"])
        self.assertFalse(report["full_checklist_fidelity"])

    def test_override_ignition_before_nominal_tig_is_measured(self):
        self.events = observations(898.616667)
        report = validate_events(self.events)
        self.assertAlmostEqual(report["actual_ignition_minus_initial_lgc_tig_s"], -2.533333)
        self.assertAlmostEqual(report["n68_request_after_actual_ignition_s"], 300)

    def test_partial_n62_register_cannot_confirm_countdown(self):
        self.event("display_confirmed", "n62_countdown")["dsky"]["R2"] = ""
        self.rejects("complete N62 TFI register")

    def test_displayed_n62_must_match_lgc_words(self):
        self.event("display_confirmed", "n62_countdown")["dsky"]["R2"] = "-01 00"
        self.rejects("displayed TFI must agree")

    def test_transient_blank_dsky_registers_are_recordable(self):
        self.event("input_up", "initial_auto_maneuver_pro")["dsky"].update(VERB="", NOUN="")
        self.assertTrue(validate_events(self.events)["checkpoint_reached"])

    def test_invalid_radar_remains_observational(self):
        self.events[-1]["radar"]["RANGEGOOD"] = "0"
        self.events[-1]["radar_geometry"]["range_good_discrete"] = False
        self.assertFalse(validate_events(self.events)["radar_measurement_available"])

    def test_sentinel_dh_cannot_claim_measurement(self):
        self.events[-1]["dsky"]["R3"] = "+99999"
        self.assertFalse(validate_events(self.events)["radar_measurement_available"])

    def test_pro_press_is_not_actual_ignition(self):
        ign = self.event("ignition", "first_positive_original_dps")
        ign["sim_elapsed_s"] = 896.15
        ign["mission_time_s"] = 368744.423737 + 896.15
        self.rejects("timestamps must not regress")

    def test_post_ignition_timing_cannot_anchor_to_pro(self):
        self.event("timing", "tig_plus_26_seconds")["sim_elapsed_s"] -= 5
        self.event("timing", "tig_plus_26_seconds")["mission_time_s"] -= 5
        self.rejects("post-ignition timing must use measured DPS ignition")

    def test_premature_enter_rejected(self):
        self.event("display_confirmed", "final_trim_return")["dsky"]["VERB"] = "06"
        self.rejects("automatic maneuver return required")

    def test_early_arming_rejected(self):
        armed = self.event("state_confirmed", "EngineArmSwitch")
        armed.update(sim_elapsed_s=750, mission_time_s=368744.423737 + 750,
                     agc_clock_s=368743.87 + 750, countdown_s=151.15)
        self.rejects("engine arming must wait")

    def test_input_hold_must_consume_simulation_time(self):
        self.event("input_up", "ignition_pro")["mission_time_s"] -= 2
        self.rejects("input holds must be included")

    def test_claimed_yaw_requires_recorded_original_inputs(self):
        for e in self.events:e["checklist_yaw_enabled"] = True
        self.rejects("original NUMPAD3 checklist yaw inputs")

    def test_exact_input_report_cannot_satisfy_requested_target_driver(self):
        from NativePDIProtocol import validate_requested_scope
        report = validate_events(self.events)
        report.update(manual_yaw_174_exercised=True, yaw_target_driver_enabled=False)
        # Both variants exercise yaw. That shared flag must not hide a mismatch.
        validate_requested_scope(report, checklist_yaw=True)
        with self.assertRaisesRegex(ValueError, "requested yaw target variant"):
            validate_requested_scope(report, yaw_target=True)

    def test_target_report_cannot_satisfy_requested_exact_input_driver(self):
        from NativePDIProtocol import validate_requested_scope
        report = validate_events(self.events)
        report.update(manual_yaw_174_exercised=True, yaw_target_driver_enabled=True)
        validate_requested_scope(report, yaw_target=True)
        with self.assertRaisesRegex(ValueError, "requested yaw target variant"):
            validate_requested_scope(report, checklist_yaw=True)

    def test_hidden_pilot_yaw_rejected(self):
        self.events[1]["event"] = "keyboard_down"
        self.rejects("no hidden pilot yaw")

    def test_n63_must_precede_override(self):
        self.event("display_confirmed", "ignition_n63")["dsky"]["NOUN"] = "62"
        self.rejects("row 150 N63 must precede")

    def test_radar_discretes_must_match_original_readbacks(self):
        self.events[-1]["radar_geometry"]["range_good_discrete"] = False
        self.rejects("radar flags and LGC data-good readbacks")

    def test_wrong_control_readback_rejected(self):
        self.event("state_confirmed", "EngineArmSwitch")["readback"] = 1
        self.rejects("readback mismatch")

    def test_hidden_v57_rejected(self):
        self.event("input_down", "request_n68")["action"] = "v57"
        self.rejects("unapproved DSKY command")

    def test_wrong_dsky_order_rejected(self):
        first = self.event("input_down", "request_n68")
        first["area_id"] = 3410
        self.rejects("complete DSKY command order")

    def test_observation_delay_required(self):
        last = self.events[-1]
        last["sim_elapsed_s"] -= 1
        last["mission_time_s"] -= 1
        self.rejects("five-second N68 observation")

    def test_nonfinite_clock_rejected(self):
        self.events[-1]["lgc_tfi_s"] = float("nan")
        self.rejects("finite lgc_tfi_s required")

    def test_recorded_erasables_must_support_tfi(self):
        self.events[-1]["tfi_low_octal"] = "0"
        self.rejects("TFI must decode")

    def test_checkpoint_must_keep_n68(self):
        self.events[-1]["dsky"]["NOUN"] = "63"
        self.rejects("N68 required")

    def test_fixed_actual_ignition_timestamp_required(self):
        self.events[-1]["actual_ignition_elapsed_s"] += 1
        self.rejects("timestamp must remain fixed")



def settling_observations(brake_seconds=4):
    """Independent analytic yaw oracle: 19 deg/s decelerates uniformly to zero."""
    import math
    action = "checklist_yaw_right_174"
    def point(t, event, angle, rate):
        radians = math.radians(angle)
        c, s = math.cos(radians), math.sin(radians)
        brake = brake_seconds is not None and 0 < t - 10 < brake_seconds
        requests = [int(brake and i in (1, 5, 10, 14)) for i in range(16)]
        return {"event": event, "action": action, "sim_elapsed_s": t,
                "keyboard_yaw_right_down": False, "aca_keyboard_yaw_right": 0,
                "aca_out_of_detent": False, "aca_proportional_roll_pitch_yaw": [0, 0, 0],
                "angular_velocity_body_rad_s": [0, -math.radians(rate), 0],
                "guidance_channel_031": 0o20000, "atca_primary_voltage_v": 28,
                "guidance_relay_k12": False, "guidance_relay_k13": False,
                "lgc_output_channel_005": 0, "lgc_output_channel_006": 85 if brake else 0,
                "pgns_jet_requests": requests.copy(), "jet_driver": requests.copy(),
                "jet_requests": requests.copy(), "rcs_jet_levels": requests.copy(),
                "yaw_integrated_deg": 174,
                "rotation_body_to_global": [[c, 0, s], [0, 1, 0], [-s, 0, c]],
                "radar_geometry": {}, "radar": {}}
    events = [point(0, "keyboard_down", 0, 0), point(10, "keyboard_up", 174, 19)]
    for i in range(1, 481):
        t = i / 60
        if brake_seconds is None:
            rate, extra = 19, 19 * t
        else:
            interval = min(t, brake_seconds)
            rate = max(0, 19 * (1 - t / brake_seconds))
            extra = 19 * interval - 19 * interval ** 2 / (2 * brake_seconds)
        events.append(point(10+t, "yaw_settling_sample", 174+extra, rate))
        if i == 12:events.append(point(10+t, "yaw_input_completed", 174+extra, rate))
    for seq, event in enumerate(events):event["seq"] = seq
    return events


class YawSettlingContract(unittest.TestCase):
    def setUp(self):
        self.events = settling_observations()

    def analyze(self):
        return analyze_yaw_settling(self.events, "checklist_yaw_right_174", 174)

    def test_neutral_release_does_not_claim_rate_settling(self):
        self.events = settling_observations(None)
        result = self.analyze()
        self.assertTrue(result["input_release_verified"])
        self.assertFalse(result["rate_settling_observed"])
        self.assertFalse(result["settled_commanded_attitude_certified"])

    def test_matrix_reports_actual_rotation_after_braking(self):
        result = self.analyze()
        self.assertTrue(result["rate_settling_observed"])
        end = result["eight_second_observation"]
        self.assertAlmostEqual(end["relative_right_yaw_projection_unwrapped_deg"], 212)
        self.assertAlmostEqual(end["orientation_error_to_fixed_start_frame_target_deg"], 38)
        self.assertAlmostEqual(result["post_release_eight_second_gyro_integral_deg"], 38)
        self.assertFalse(result["settled_commanded_attitude_certified"])

    def test_early_release_sample_is_separate_from_rate_dwell(self):
        result = self.analyze()
        self.assertAlmostEqual(result["release_verified_0_2s"]["after_release_s"], .2)
        self.assertGreater(result["first_low_rate_observation"]["after_release_s"], 3.9)
        self.assertGreater(result["rate_dwell_verified"]["after_release_s"], 4.9)
        self.assertGreater(result["release_verified_0_2s"]["yaw_rate_deg_s"], 18)

    def test_missing_per_step_observation_rejected(self):
        self.events.pop(20)
        with self.assertRaisesRegex(ValueError, "480 per-step"):
            self.analyze()

    def test_wrong_step_timestamp_rejected(self):
        self.events[20]["sim_elapsed_s"] += .01
        with self.assertRaisesRegex(ValueError, "continuous eight-second"):
            self.analyze()

    def test_non_rotation_matrix_rejected(self):
        self.events[20]["rotation_body_to_global"][0][0] = 2
        with self.assertRaisesRegex(ValueError, "orthonormal"):
            self.analyze()

    def test_reflected_matrix_rejected(self):
        self.events[0]["rotation_body_to_global"] = [[-1,0,0],[0,1,0],[0,0,1]]
        with self.assertRaisesRegex(ValueError, "proper attitude"):
            self.analyze()

    def test_stale_aca_output_rejected(self):
        self.events[20]["aca_proportional_roll_pitch_yaw"][2] = -.5
        with self.assertRaisesRegex(ValueError, "neutral original input"):
            self.analyze()

    def test_lgc_output_must_support_jet_request(self):
        self.events[20]["pgns_jet_requests"][0] = 1
        with self.assertRaisesRegex(ValueError, "LGC output channels"):
            self.analyze()



def target_observations(target=174, swing_degrees=0):
    """Independent orientation oracle with a nonidentity starting attitude."""
    import math
    from NativePDIProtocol import matrix_product, right_yaw_matrix
    action = "target_yaw_right_174"
    def pitch(angle):
        c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
        return [[1,0,0],[0,c,-s],[0,s,c]]
    start = matrix_product(pitch(23), right_yaw_matrix(-57))
    def point(t, event):
        angle = target*min(1,t/2)
        # Swing about body X followed by body-Y twist. It must not turn the
        # diagnostic yaw certificate into a full fixed-frame pose certificate.
        relative = matrix_product(pitch(swing_degrees*min(1,t/2)), right_yaw_matrix(angle))
        return {"event":event, "action":action, "sim_elapsed_s":t, "area_id":0x51, "checklist_row":169,
                "rotation_body_to_global":matrix_product(start,relative),
                "angular_velocity_body_rad_s":[0, -math.radians(target/2) if t<2 else 0,0],
                "keyboard_yaw_right_down":0<t<1.9, "aca_keyboard_yaw_right":0,
                "aca_out_of_detent":False, "aca_proportional_roll_pitch_yaw":[0,0,0],
                "guidance_channel_031":0, "radar":{}, "radar_geometry":{}}
    events = [point(0,"yaw_target_start"), point(0,"keyboard_down")]
    for i in range(1,721):
        t = i/60
        events.append(point(t,"yaw_target_sample"))
        if i==114:events.append(point(t,"keyboard_up"))
        if i==240:events.append(point(t,"yaw_target_acquired"))
    events.append(point(12,"yaw_target_observation_end"))
    for seq,e in enumerate(events):e["seq"]=seq
    return events


class YawTargetContract(unittest.TestCase):
    def setUp(self):self.events=target_observations()
    def analyze(self):
        from NativePDIProtocol import analyze_yaw_target
        return analyze_yaw_target(self.events,"target_yaw_right_174",174)
    def test_nonidentity_start_and_174_degree_quaternion(self):
        result=self.analyze()
        self.assertTrue(result["diagnostic_yaw_target_validated"])
        self.assertAlmostEqual(result["terminal_dwell"]["relative_right_yaw_twist_unwrapped_deg"],174)
        self.assertAlmostEqual(result["terminal_dwell"]["relative_quaternion_wxyz"][0],0.05233595624)
        self.assertFalse(result["full_fixed_frame_attitude_certified"])
        self.assertFalse(result["diagnostic_criterion"]["operational_flight_requirement"])
    def test_pgns_pitch_swing_is_reported_separately(self):
        self.events=target_observations(swing_degrees=5)
        result=self.analyze()
        self.assertAlmostEqual(result["terminal_dwell"]["yaw_target_error_deg"],0)
        self.assertAlmostEqual(result["terminal_dwell"]["full_rotation_error_to_pure_start_frame_yaw_deg"],5)
    def test_zero_rate_does_not_certify_212_degree_overshoot(self):
        self.events=target_observations(target=212)
        with self.assertRaisesRegex(ValueError,"terminal quaternion yaw"):self.analyze()
    def test_low_rate_outside_angle_tolerance_rejected(self):
        self.events=target_observations(target=175.01)
        with self.assertRaisesRegex(ValueError,"terminal quaternion yaw"):self.analyze()
    def test_brief_terminal_rate_spike_invalidates_dwell(self):
        import math
        next(e for e in self.events if e["event"]=="yaw_target_sample" and e["sim_elapsed_s"]==3.5)["angular_velocity_body_rad_s"][1]=-math.radians(.2)
        with self.assertRaisesRegex(ValueError,"low measured yaw rate"):self.analyze()
    def test_aca_not_neutral_during_dwell_rejected(self):
        next(e for e in self.events if e["event"]=="yaw_target_acquired")["aca_out_of_detent"]=True
        with self.assertRaisesRegex(ValueError,"neutral original ACA"):self.analyze()
    def test_missing_per_step_sample_rejected(self):
        self.events.pop(150)
        with self.assertRaisesRegex(ValueError,"continuous per-step"):self.analyze()
    def test_left_yaw_correction_is_rejected(self):
        self.events[1]["area_id"]=0x4f
        with self.assertRaisesRegex(ValueError,"right-only original ACA"):self.analyze()


if __name__ == "__main__":
    unittest.main()
