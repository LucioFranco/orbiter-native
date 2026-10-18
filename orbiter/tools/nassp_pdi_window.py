#!/usr/bin/env python3
"""Stage original PDI, display its uninterrupted ACA diagnostic and retain its host.

--self-test uses synthetic SDL events; it is not human or physical-controller QA.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from nassp_demo import ROOT, cmake_cache, verify_build, verify_data, verify_sources, DataError
from stage_nassp_mission import stage

sys.path.insert(0,str(ROOT/'Tests'))
from NativePDIProtocol import preflight, require, vessels
from NativePDIUninterruptedACA import validate_aca_events

SCENARIO='Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m.scn'
CHECKS=('automation_input_isolation_checked','same_host_paused_handoff','pause_frozen_checked',
        'unpause_checked','manual_keyboard_aca_checked','manual_neutral_checked','focus_release_checked',
        'manual_repause_checked','continuation_guard_checked','guard_repause_checked',
        'normal_close_checked','escape_close_checked')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,default=ROOT/'build/native-v9')
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--repeat',type=int,choices=(1,2),default=1)
    args=parser.parse_args()
    if args.repeat!=1 and not args.self_test:parser.error('--repeat requires --self-test')
    build=args.build.resolve();cache=cmake_cache(build)
    data=Path(cache['NASSP_DATA_DIR']).resolve();profile=verify_data(data)['profile']
    provenance=verify_sources(Path(cache['NASSP_ADDON_DIR']),profile);verify_build(build)
    probe=build/'native_pdi_protocol_probe';require(probe.is_file(),'build native_pdi_protocol_probe first')
    source=data/'Scenarios/Project Apollo - NASSP'/SCENARIO
    reports=[]
    for index in range(args.repeat):
        runtime=build/(f'runtime-pdi-window-test-{index+1}' if args.self_test else 'runtime-pdi-window')
        startup_config=runtime/'Orbiter.cfg'
        manual_config=b'; Owned PDI manual-step automation policy\nStartPaused = FALSE\n'
        require(not startup_config.exists() or startup_config.read_bytes()==manual_config,
                'manual PDI window requires its owned StartPaused FALSE config; preserve foreign config and use a fresh runtime')
        # Do not let staging or the diagnostic overwrite a user's controller map.
        for name in ('LM','CSM'):
            require(not (runtime/f'Config/ProjectApollo/Vesim/{name}.native.cfg').exists(),
                    'controller config exists; use a fresh isolated runtime')
        stage(argparse.Namespace(build=build,addon=data,profile=profile,
            textures=Path(cache['ORBITER_TEXTURE_DIR']) if cache.get('ORBITER_TEXTURE_DIR') else None,
            scenario=SCENARIO,output=runtime),ROOT)
        startup_config.write_bytes(manual_config)
        initial=preflight(source,runtime);started=time.monotonic()
        env=dict(os.environ,ORBITER_PDI_CAPTURE_DIR=str(runtime/'window-captures'),PYTHONDONTWRITEBYTECODE='1')
        option='--pilot-aca-window-test' if args.self_test else '--pilot-aca-window'
        command=[str(probe),str(runtime),'1800',option]
        print('Launching original uninterrupted PDI; close window to cancel. '+str(runtime),flush=True)
        with (runtime/'PDIWindow.log').open('w') as log:
            result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,env=env,
                timeout=1200 if args.self_test else None)
        require(result.returncode==0,'window diagnostic failed or cancelled; inspect '+str(runtime/'PDIWindow.log'))
        events=[json.loads(l) for l in (runtime/'PDIProtocol.jsonl').read_text().splitlines()]
        aca=validate_aca_events(events)
        live=[json.loads(l) for l in (runtime/'PDIWindow.jsonl').read_text().splitlines()]
        handoff=[e for e in live if e['event']=='same_host_paused_handoff']
        require(len(handoff)==1 and not handoff[0]['running'] and not handoff[0]['virtual_binding_active'] and
                handoff[0]['aca_yaw']==0 and not handoff[0]['aca_out_of_detent'],'paused neutral live handoff required')
        final=aca['uninterrupted_aca']['final_sample']
        require(handoff[0]['cycles']==final['lgc_cycle_counter'] and
                abs(handoff[0]['mission_time']-final['mission_time_s'])<1e-8,'no processor/mission-time advance or reset at handoff')
        if args.self_test:
            require(set(CHECKS)<=set(e['event'] for e in live),'all SDL lifecycle observations required')
        require(preflight(source,runtime)==initial,'original mission inputs must remain unchanged')
        saved=vessels((runtime/'Scenarios/NativePDIPilotHandoff.scn').read_text())
        require({n:s[0] for n,s in saved.items()}=={n:s[0] for n,s in vessels(source.read_text()).items()},'all original vessels required')
        require(not (runtime/'Config/ProjectApollo/Vesim/LM.native.cfg').exists(),'virtual binding file must be removed')
        require('ERROR:' not in (runtime/'Orbiter.log').read_text(errors='replace'),'clean native simulation log required')
        report={'validation_passed':True,'profile':profile,'runtime':str(runtime),'launch_argv':command,
            'source_preflight':provenance,'original_inputs':initial,'uninterrupted_aca':aca['uninterrupted_aca'],
            'live_window_observations':live,'synthetic_SDL_lifecycle_test':args.self_test,
            'human_control_validated':False,'physical_controller_validated':False,'landing_validated':False,
            'wall_seconds':time.monotonic()-started,
            'sha256':{str(p.relative_to(runtime)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [runtime/'PDIProtocol.jsonl',runtime/'PDIWindow.jsonl',*sorted((runtime/'window-captures').glob('*.ppm'))]}}
        (runtime/'pdi-window-verification.json').write_text(json.dumps(report,indent=2)+'\n')
        reports.append({'runtime':str(runtime),'validation_passed':True,'wall_seconds':report['wall_seconds']})
        print(json.dumps(reports[-1]),flush=True)
    print(json.dumps({'profile':profile,'runs':reports,'repeated_launch_validated':args.repeat==2}),flush=True)

if __name__=='__main__':
    try:main()
    except (DataError,ValueError,OSError,subprocess.SubprocessError) as error:
        sys.exit('PDI window: '+str(error))
