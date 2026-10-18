"""Mutation contracts for the uninterrupted diagnostic; synthetic data is not flight evidence."""
import math
import unittest
from NativePDIUninterruptedACA import ACA_YAW_FRACTION, CONTROLS, PHASES, measure_aca_samples


def fixture():
    rows, start = [], 1000.0
    for phase, frames in PHASES:
        for tick in range(frames+1):
            t=start+tick/60
            active=phase=='yaw_pulse' and tick>0 or phase=='neutral_release' and tick==0
            raw=-20 if phase=='yaw_pulse' and tick>=6 or phase=='neutral_release' and tick<6 else 0
            if phase=='neutral_baseline':yaw=.025
            elif phase=='yaw_pulse':yaw=.025+tick/18
            else:yaw=.025+max(0,1-tick/60)
            angle=(t-1000)*.002;c,s=math.cos(angle),math.sin(angle)
            thrust=phase=='yaw_pulse' and tick>=13 or phase=='neutral_release' and 0<tick<61
            torque=-100 if phase=='yaw_pulse' or tick<7 else 100
            rows.append({'event':'handoff_aca_sample','action':phase,'aca_phase_tick':tick,
                         'handoff_aca_enabled':True,'pilot_driver_enabled':True,'pilot_descent_enabled':True,
                         'yaw_target_driver_enabled':False,'sim_elapsed_s':t,'mission_time_s':t+50000,
                         'lgc_cycle_counter':100000000+round((t-1000)*85333),
                         'ground_contact':False,'lgc_altitude_ft':1900-(t-1000)*40,'dps_level':.4,
                         'dsky':{'PROG':'64'},'controls':dict(CONTROLS),
                         'guidance_channel_031':(1<<12)|((1<<14) if active else 0),
                         'guidance_relay_k12':False,'guidance_relay_k13':False,'gasta_powered':True,
                         'rcs_jet_levels':[int(thrust)]+[0]*15,'pgns_jet_requests':[int(thrust)]+[0]*15,
                         'jet_driver':[int(thrust)]+[0]*15,
                         'angular_velocity_body_rad_s':[0,-math.radians(yaw),0],
                         'rcs_torque_body_nm':[0,torque if thrust else 0,0],'pgns_fdai_rpy_deg':[0,0,angle],
                         'rotation_body_to_global':[[c,0,s],[0,1,0],[-s,0,c]],
                         'pilot_aca_axis_s16':14823 if phase=='yaw_pulse' else 0,
                         'aca_proportional_roll_pitch_yaw':[0,0,ACA_YAW_FRACTION if active else 0],
                         'aca_out_of_detent':bool(active),'lgc_rhc_yaw_raw_s32':raw,
                         'lgc_rhc_yaw_count':-19 if raw else 0})
        start+=frames/60
    return rows


class UninterruptedACAContract(unittest.TestCase):
    def setUp(self):self.rows=fixture()
    def rejects(self,pattern):
        with self.assertRaisesRegex(ValueError,pattern):measure_aca_samples(self.rows)
    def test_complete_control_trace_and_async_counter_reads_pass(self):
        result=measure_aca_samples(self.rows)
        self.assertEqual(result['samples'],321)
        self.assertFalse(result['faithful_resume_validated'])
    def test_reset_cycle_history_is_rejected(self):
        self.rows[100]['lgc_cycle_counter']=0;self.rejects('cycle history')
    def test_frame_omission_is_rejected(self):
        self.rows.pop();self.rejects('321-frame')
    def test_shortened_pulse_is_rejected(self):
        self.rows[65]['sim_elapsed_s']+=.01;self.rejects('fixed-duration')
    def test_automatic_mode_is_rejected(self):
        self.rows[20]['controls']['ModeControlPGNSSwitch']=2;self.rejects('control readbacks')
    def test_retained_neutral_stick_input_is_rejected(self):
        self.rows[-1]['aca_proportional_roll_pitch_yaw'][2]=ACA_YAW_FRACTION;self.rejects('neutral-release')
    def test_counter_that_never_clears_is_rejected(self):
        self.rows[-1].update(lgc_rhc_yaw_raw_s32=-20,lgc_rhc_yaw_count=-19);self.rejects('RHC read/release')
    def test_missing_jet_driver_response_is_rejected(self):
        for e in self.rows:e['jet_driver']=[0]*16
        self.rejects('command-to-jet')
    def test_missing_braking_response_is_rejected(self):
        for e in self.rows:
            if e['action']=='neutral_release':e['rcs_torque_body_nm'][1]=0
        self.rejects('braking torque')
    def test_no_measurable_motion_is_rejected(self):
        for e in self.rows:e['angular_velocity_body_rad_s'][1]=-.0001
        self.rejects('braking torque|right-yaw response')
    def test_unsettled_neutral_release_is_rejected(self):
        for e in self.rows[-61:]:e['angular_velocity_body_rad_s'][1]=-.02
        self.rejects('neutral damping')
    def test_manual_landing_program_is_rejected(self):
        self.rows[-1]['dsky']['PROG']='66';self.rejects('guided descent')
    def test_nonphysical_attitude_matrix_is_rejected(self):
        self.rows[-1]['rotation_body_to_global'][0][0]=2;self.rejects('orthonormal')


if __name__=='__main__':unittest.main()
