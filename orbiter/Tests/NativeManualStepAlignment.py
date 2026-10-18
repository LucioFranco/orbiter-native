"""Check actual time/AGC boundaries, not nominal loop counts, on an original mission."""
from pathlib import Path
import hashlib,json,subprocess,sys,tempfile
from NativePDIProtocol import SCENARIO,field,vessels,require
probe,build,data,output=map(lambda p:Path(p).resolve(),sys.argv[1:5]);engine=Path(__file__).resolve().parents[1];output.mkdir(parents=True,exist_ok=True);run=Path(tempfile.mkdtemp(prefix='run-',dir=output));config=engine/'Tests/Fixtures/NativeScenarioCompatibility/Orbiter.cfg';results={}
# Locked stock/historical boundaries at the scenario's saved precision.
expected={1:('40422.8317623667','1422'),301:('40422.8318202371','428088'),319:('40422.8318237093','453688'),559:('40422.8318700056','795022')}
def stage(name):
 root=run/name;cmd=[sys.executable,str(engine/'tools/stage_nassp_manual.py'),str(build),'--addon',str(data),'--profile','v9','--scenario',SCENARIO+'.scn','--output',str(root),'--config',str(config)];p=subprocess.run(cmd,capture_output=True,text=True);require(p.returncode==0,p.stderr);return root
def boundary(path,n):
 text=path.read_text();ships=vessels(text);mjd,counter=expected[n];require(field(text,'Date').split()[-1]==mjd,'actual serialized time boundary '+str(n));counters={v:field(ships[v][1].split('AGC_BEGIN',1)[1].split('AGC_END',1)[0],'CYCLECOUNTER') for v in ['Columbia','Eagle']};require(set(counters.values())=={counter},'actual AGC cycle boundary '+str(n));return {'MJD':mjd,'AGC_counters':counters,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
try:
 for name,n,options in [('neutral301',301,[]),('neutral319',319,[]),('neutral559',559,[]),('pulse559',559,['--aca'])]:
  root=stage(name);cmd=[str(probe),str(root),SCENARIO,str(n),*options,'--pause-after'];p=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(root/'execution.log').write_text(p.stdout+p.stderr);require(p.returncode==0,p.stderr);rows=[json.loads(x) for x in (root/'parity-state.jsonl').read_text().splitlines()];require(rows[0]['sim_time_s']==0 and not rows[0]['physics']['StartPaused_config'],'manual zero-step initial state');running=[r for r in rows if r['phase'] not in ['loaded','pause_applied']];require(len(running)==n and all(r['running'] and r['advanced_s']>0 for r in running),'every requested step must advance');result={'final':boundary(root/'Scenarios/ParityFinal.scn',n),'baseline':boundary(root/'Scenarios/ParityBaseline.scn',1),'actual_elapsed_seconds':rows[-1]['sim_time_s'],'positive_advances':len(running)}
  if options:result['pulse_end']=boundary(root/'Scenarios/ParityPulse.scn',319);require(sum(r['keyboard_numpad3'] for r in running)==18,'pulse302 through319')
  results[name]=result;print(name,'real time/AGC alignment PASS',flush=True)
 root=stage('reject-startpaused');p=root/'Orbiter.cfg';p.write_bytes(p.read_bytes().replace(b'StartPaused = FALSE',b'StartPaused = TRUE'));cmd=[str(probe),str(root),SCENARIO,'301'];p=subprocess.run(cmd,capture_output=True,text=True,timeout=30);(root/'execution.log').write_text(p.stdout+p.stderr);require(p.returncode!=0 and 'StartPaused=FALSE' in p.stderr,'reject incompatible startup instead of false successful loop count');require(not (root/'Scenarios/ParityLoaded.scn').exists(),'reject before loading/advancing the world');results['reject-startpaused']={'returncode':p.returncode,'rejected_before_world_load':True}
finally:(run/'result.json').write_text(json.dumps({'run':str(run),'cases':results,'expected_boundary_provenance':'Locked stock Windows step1/301/319/559; raw historical comparisons retained','passed':len(results)==5},indent=2)+'\n');print('Evidence:',run,flush=True)
