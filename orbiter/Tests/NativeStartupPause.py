"""Separate real-host StartPaused cases; preserve red/green runs and input policy."""
from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,sys,tempfile
exe,fixture,callback,output=map(lambda p:Path(p).resolve(),sys.argv[1:5]);graphics=Path(sys.argv[5]).resolve() if len(sys.argv)==6 else None
output.mkdir(parents=True,exist_ok=True);run=Path(tempfile.mkdtemp(prefix='run-',dir=output));cases={};modes=['true-unfixed','true-fixed','true-command-fixed','false','default'] if graphics is None else ['true-fixed','false']
for mode in modes:
 root=run/mode;shutil.copytree(fixture,root,symlinks=True);cfg=root/'Orbiter.cfg'
 if cfg.exists():cfg.unlink()
 if mode!='default':cfg.write_text('StartPaused = '+('TRUE' if mode.startswith('true-') else 'FALSE')+'\n'+('FixedStep = 0.016666666666666666\n' if mode in ['true-fixed','true-command-fixed'] else ''))
 command=[str(exe),str(root),str(callback),mode]+([str(graphics)] if graphics else []);p=subprocess.run(command,capture_output=True,text=True,timeout=90);(root/'execution.log').write_text(p.stdout+p.stderr);cases[mode]={'command':command,'returncode':p.returncode,'output':p.stdout+p.stderr}
 if (root/'startup-pause-result.json').exists():cases[mode]['result']=json.loads((root/'startup-pause-result.json').read_text())
 print(mode,'PASS' if p.returncode==0 else 'FAIL',flush=True)
record={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'cases':cases,'passed':all(c['returncode']==0 for c in cases.values()),'graphics':bool(graphics),'run':str(run),'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()};(run/'result.json').write_text(json.dumps(record,indent=2)+'\n');print('Evidence:',run,flush=True);assert record['passed'],'StartPaused regression failures preserved'
