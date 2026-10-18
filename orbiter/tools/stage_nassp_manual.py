#!/usr/bin/env python3
"""Stage a fresh original mission for a driver that owns every simulation step."""
import argparse,hashlib,json,re
from pathlib import Path
from types import SimpleNamespace
from stage_nassp_mission import stage
from nassp_demo import ROOT,PROFILES,DataError

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('build',type=Path);p.add_argument('--addon',type=Path,required=True);p.add_argument('--profile',choices=PROFILES,required=True);p.add_argument('--scenario',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--textures',type=Path);p.add_argument('--config',type=Path,help='Optional verified reference config; only startup/fixed-step policy changes')
 a=p.parse_args();out=a.output.resolve()
 if out.exists():p.error('Preserve existing runtime/evidence; select a fresh output path')
 raw=a.config.read_bytes() if a.config else b''
 def policy(data,key,value):
  pattern=rb'(?mi)^[ \t]*'+key+rb'[ \t]*=[^\r\n]*';matches=list(re.finditer(pattern,data))
  if len(matches)>1:raise DataError('Ambiguous repeated '+key.decode()+' in supplied config')
  line=key+b' = '+value
  return re.sub(pattern,line,data) if matches else data+b'\r\n'+line+b'\r\n'
 configured=policy(policy(raw,b'StartPaused',b'FALSE'),b'FixedStep',b'0.016666666666666666')
 stage(SimpleNamespace(build=a.build,addon=a.addon,profile=a.profile,textures=a.textures,scenario=a.scenario,output=out),ROOT)
 (out/'Orbiter.cfg').write_bytes(configured);record={'manual_step_policy':'StartPaused FALSE: no implicit initialization, no resume and no extra step','fixed_step_seconds':'0.016666666666666666','input_config':str(a.config.resolve()) if a.config else None,'input_config_sha256':hashlib.sha256(raw).hexdigest(),'runtime_config_sha256':hashlib.sha256(configured).hexdigest(),'scenario_bytes_modified':False};(out/'manual-step-policy.json').write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps(record,indent=2))
if __name__=='__main__':main()
