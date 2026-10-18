"""Published-envelope and complete-input contracts; synthetic data is not flight proof."""
import copy
import unittest
from NativePDIPilot import radar_acceptance, audit_keys, V57_KEYS
from NativePDIPersistence import continuation_comparison


def decision():
    return {"dsky": {"PROG": "63", "VERB": "16", "NOUN": "68", "R3": "-01832"},
            "lgc_altitude_ft": 34000, "lgc_flagword11_octal": "40",
            "radar": {"RANGEGOOD": "1", "VELOCITYGOOD": "1", "RANGE": "37000"},
            "radar_geometry": {"range_good_discrete": True, "velocity_good_discrete": True,
                               "position1_discrete": True, "position2_discrete": False, "radar_test_switch": 1}}


class RadarEnvelopeContract(unittest.TestCase):
    def setUp(self):
        self.event = decision()

    def rejects(self, message):
        with self.assertRaisesRegex(ValueError, message):
            radar_acceptance(self.event)

    def test_published_plateau_includes_both_endpoints_and_both_signs(self):
        for height in (27500, 34000, 40700):
            for difference in ("+17500", "-17500"):
                with self.subTest(height=height, difference=difference):
                    self.event["lgc_altitude_ft"] = height
                    self.event["dsky"]["R3"] = difference
                    result = radar_acceptance(self.event)
                    self.assertEqual(result["dh_ft"], int(difference))
                    self.assertFalse(result["source_chart"]["extrapolation"])

    def test_no_altitude_extrapolation(self):
        for height in (27499, 40701, float("nan"), float("inf")):
            self.event["lgc_altitude_ft"] = height
            self.rejects("outside published")

    def test_one_foot_over_either_signed_limit_is_rejected(self):
        for difference in ("+17501", "-17501"):
            self.event["dsky"]["R3"] = difference
            self.rejects("exceeds published")

    def test_incomplete_dsky_is_not_a_radar_measurement(self):
        for value in ("", "-01 32", "01832", "-1832", "nan", "-1e003"):
            self.event["dsky"]["R3"] = value
            self.rejects("complete signed")

    def test_dsky_sentinel_is_not_a_measurement(self):
        for value in ("+99999", "-99999"):
            self.event["dsky"]["R3"] = value
            self.rejects("sentinel")

    def test_n63_altitude_cannot_be_read_as_n68_dh(self):
        self.event["dsky"].update(VERB="06", NOUN="63")
        self.rejects("P63 V16N68")

    def test_radar_good_flags_must_agree_with_original_lgc_inputs(self):
        for group, key, value in (("radar", "RANGEGOOD", "0"), ("radar", "VELOCITYGOOD", "0"),
                                  ("radar_geometry", "range_good_discrete", False),
                                  ("radar_geometry", "velocity_good_discrete", False)):
            self.event = decision()
            self.event[group][key] = value
            self.rejects("data-good agreement")

    def test_radar_test_mode_cannot_satisfy_operating_requirement(self):
        self.event["radar_geometry"]["radar_test_switch"] = 0
        self.rejects("operating position")

    def test_hover_position_cannot_satisfy_descent_acceptance(self):
        self.event["radar_geometry"].update(position1_discrete=False, position2_discrete=True)
        self.rejects("operating position")

    def test_nonpositive_or_nonfinite_range_cannot_satisfy_data_good(self):
        for value in ("0", "-1", "nan", "inf"):
            self.event["radar"]["RANGE"] = value
            self.rejects("positive finite")

    def test_already_enabled_incorporation_is_not_a_pre_v57_decision(self):
        self.event["lgc_flagword11_octal"] = "240"
        self.rejects("inhibited before V57")


class PilotInputContract(unittest.TestCase):
    def setUp(self):
        self.events = []
        for index, key in enumerate(V57_KEYS):
            self.events.extend([{"seq": index*2, "event": "input_down", "action": "enable_lr_v57",
                                 "area_id": key, "sim_elapsed_s": index*.4},
                                {"seq": index*2+1, "event": "input_up", "action": "enable_lr_v57",
                                 "area_id": key, "sim_elapsed_s": index*.4+.2}])

    def check(self):
        audit_keys(self.events, {"enable_lr_v57": V57_KEYS})

    def test_original_v57_sequence_and_hold(self):
        self.check()

    def test_extra_v57_is_rejected(self):
        self.events.extend(copy.deepcopy(self.events))
        with self.assertRaisesRegex(ValueError, "complete pilot DSKY"):
            self.check()

    def test_unreleased_key_is_rejected(self):
        del self.events[-1]
        with self.assertRaisesRegex(ValueError, "complete pilot DSKY"):
            self.check()

    def test_wrong_key_order_is_rejected(self):
        self.events[2]["area_id"] = 3412
        with self.assertRaisesRegex(ValueError, "complete pilot DSKY"):
            self.check()

    def test_incorrect_input_hold_is_rejected(self):
        self.events[1]["sim_elapsed_s"] = 1
        with self.assertRaisesRegex(ValueError, "paired measured"):
            self.check()


class NeutralResumeContract(unittest.TestCase):
    def setUp(self):
        sample = {"event": "neutral_resume_sample", "action": "uninterrupted", "ground_contact": False,
                  "lgc_altitude_ft": 1900, "pilot_aca_axis_s16": 0, "aca_out_of_detent": False,
                  "aca_proportional_roll_pitch_yaw": [0, 0, 0], "radar": {"RANGE": "1920",
                  "RANGEGOOD": "1", "VELOCITYGOOD": "1", "ANTENNAANGLE": "0"},
                  "radar_geometry": {"range_good_discrete": True, "velocity_good_discrete": True,
                  "position1_discrete": False, "position2_discrete": True},
                  "crosspointers": [[18.39, .05571], [18.39, .05571]],
                  "dps_gimbals": [{"position": -.06, "command": -1, "motor_running": 1},
                                  {"position": -.08, "command": 1, "motor_running": 1}],
                  "dps_level": .42355, "angular_velocity_body_rad_s": [0, 0, 0],
                  "rotation_body_to_global": [[1, 0, 0], [0, 1, 0], [0, 0, 1]], "rcs_jet_levels": [0]*16}
        self.reference = [dict(copy.deepcopy(sample), neutral_trace_tick=tick, mission_time_s=1000+tick/60)
                          for tick in range(31)]
        self.resumed = [dict(copy.deepcopy(e), action="resumed") for e in self.reference]

    def result(self):
        return continuation_comparison(self.reference, self.resumed)

    def test_matching_neutral_continuation(self):
        self.assertTrue(self.result()["continuation_matches_observation_bounds"])

    def test_first_step_thrust_dropout_is_rejected(self):
        self.resumed[1]["dps_level"] = 0
        self.assertFalse(self.result()["continuation_matches_observation_bounds"])

    def test_first_step_range_disagreement_is_rejected(self):
        self.resumed[1]["radar"]["RANGE"] = "1920.5"
        self.assertFalse(self.result()["continuation_matches_observation_bounds"])

    def test_range_flag_disagreement_is_rejected(self):
        self.resumed[1]["radar"]["RANGEGOOD"] = "0"
        self.assertFalse(self.result()["continuation_matches_observation_bounds"])

    def test_either_pointer_disagreement_is_rejected(self):
        for side in range(2):
            changed = copy.deepcopy(self.resumed)
            changed[0]["crosspointers"][side][0] = 0
            self.assertFalse(continuation_comparison(self.reference, changed)["continuation_matches_observation_bounds"])

    def test_gimbal_motor_and_command_disagreement_is_rejected(self):
        for field in ("motor_running", "command"):
            changed = copy.deepcopy(self.resumed)
            changed[1]["dps_gimbals"][1][field] = 0
            self.assertFalse(continuation_comparison(self.reference, changed)["continuation_matches_observation_bounds"])

    def test_non_neutral_input_cannot_be_compared_as_neutral(self):
        self.resumed[1]["pilot_aca_axis_s16"] = 14823
        with self.assertRaisesRegex(ValueError, "neutral original-ACA"):
            self.result()

    def test_missing_or_unpaired_first_step_is_rejected(self):
        del self.resumed[1]
        with self.assertRaisesRegex(ValueError, "31 neutral"):
            self.result()


if __name__ == "__main__":
    unittest.main()
