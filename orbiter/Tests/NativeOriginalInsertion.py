"""Original Apollo 11 insertion load, coast, and save/reload gate.

This proves original-scenario compatibility and native execution, not trajectory
parity with a Windows reference. No scenario state or control sequence is tuned.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import subprocess
import sys

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("executable",type=Path);parser.add_argument("runtime",type=Path);parser.add_argument("source",type=Path)
parser.add_argument("--coast-seconds",type=int,default=60)
args=parser.parse_args();assert args.coast_seconds>=60
exe,runtime,source=(p.resolve() for p in (args.executable,args.runtime,args.source))
coast_seconds=args.coast_seconds
scenario="Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion"
staged=runtime/"Scenarios"/(scenario+".scn")
assert staged.read_bytes()==source.read_bytes(),"scenario must be byte-identical to upstream source"

def run(name,steps,save):
    result=subprocess.run([str(exe),"--root",str(runtime),"--scenario",name,"--steps",str(steps),"--dt","0.016666666666666666","--save",save],text=True,capture_output=True,timeout=120)
    (runtime/(save+".log")).write_text(result.stdout+result.stderr)
    assert result.returncode==0,"process exit " + str(result.returncode) + ": " + (result.stdout+result.stderr)[-8000:]
    log=(runtime/"Orbiter.log").read_text(errors="replace")
    assert "ERROR:" not in log, "scenario load/propagation must not use a failed module fallback: " + log[-8000:]
    assert not re.search(r"(?:could not|failed to|cannot) load.*(?:module|ephemeris)",log,re.I),log[-5000:]
    return (runtime/"Scenarios"/(save+".scn")).read_text()

def vessels(text):
    ships=text.split("BEGIN_SHIPS",1)[1].split("END_SHIPS",1)[0]
    return {m[0]:(m[1].replace("\\","/"),m[2]) for m in re.findall(r"^([^\s:]+):([^\n]+)\n(.*?)^END$",ships,re.M|re.S)}

def scalar(text,key):
    return float(re.search(r"^\s*"+re.escape(key)+r"\s+([^\n]+)",text,re.M)[1])

def vector(text,key):
    return [float(x) for x in re.search(r"^\s*"+key+r"\s+([^\n]+)",text,re.M)[1].split()]

original=source.read_text()
initial=run(scenario,0,"OriginalInsertionLoaded")
expected,loaded=vessels(original),vessels(initial)
assert {k:v[0] for k,v in expected.items()}=={k:v[0] for k,v in loaded.items()},"every original vessel and class must load"
assert len(loaded)==5,"original insertion contains Columbia, MCC, floodlight and two discarded stages"
assert abs(scalar(initial,"Date MJD")-scalar(original,"Date MJD"))<1e-9,"original epoch preserved"
c0,c1=expected["Columbia"][1],loaded["Columbia"][1]
assert abs(scalar(c0,"MISSNTIME")-scalar(c1,"MISSNTIME"))<1e-9,"mission time preserved on load"
for field in ("RPOS","RVEL"):
    assert math.dist(vector(c0,field),vector(c1,field))<1,"initial state preserved to scenario precision"
advanced=run(scenario,coast_seconds*60,"OriginalInsertionCoast")
ships=vessels(advanced)
# Saturn::clbkPreStep deletes the discarded S-II and interstage below 1 km
# through its original KillAlt callbacks. A sustained coast crosses that point;
# Columbia, MCC and Floodlight must remain, and no unknown vessel may appear.
removed = set(expected) - set(ships)
assert removed <= {"AS-506-STG2", "AS-506-INTSTG"}, "only upstream discarded-stage cleanup is allowed"
assert not set(ships) - set(expected), "coast unexpectedly created vessels"
if coast_seconds == 60:
    assert not removed, "all five vessels must survive the short coast"
c2=ships["Columbia"][1]
assert abs(scalar(c2,"MISSNTIME")-scalar(c1,"MISSNTIME")-coast_seconds)<1e-5,"requested coast seconds of mission time"
assert abs(scalar(advanced,"Date MJD")-scalar(initial,"Date MJD")-coast_seconds/86400)<1e-9,"world and mission clocks agree"
for name,(_,block) in ships.items():
    for field in ("RPOS","RVEL"):
        if re.search(r"^\s*"+field+r"\s",block,re.M): assert all(math.isfinite(x) for x in vector(block,field)),name+" has nonfinite state"
assert math.dist(vector(c1,"RPOS"),vector(c2,"RPOS"))>100000,"Columbia propagates around Earth"
assert abs(math.dist(vector(c2,"RPOS"),[0,0,0])/math.dist(vector(c1,"RPOS"),[0,0,0])-1)<0.05,"short coast remains in the initial orbital band"
reloaded=run("OriginalInsertionCoast",0,"OriginalInsertionReloaded")
c3=vessels(reloaded)["Columbia"][1]
assert set(vessels(reloaded))==set(ships),"save/reload retains every surviving vessel"
assert abs(scalar(c3,"MISSNTIME")-scalar(c2,"MISSNTIME"))<1e-9
def agc(text):
    state=text.split("AGC_BEGIN",1)[1].split("AGC_END",1)[0]
    return [line for line in state.splitlines() if not line.strip().startswith("CYCLECOUNTER ")]
assert agc(c3)==agc(c2),"AGC memory/channels retained; upstream does not restore cycle counter"
continued=run("OriginalInsertionReloaded",120,"OriginalInsertionContinued")
assert abs(scalar(vessels(continued)["Columbia"][1],"MISSNTIME")-scalar(c3,"MISSNTIME")-2)<1e-5
report={"scenario":scenario+".scn","source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),"scenario_modified":False,
        "vessels":sorted(loaded),"surviving_vessels":sorted(ships),"discarded_stages_removed":sorted(removed),"coast_seconds":coast_seconds,"resume_seconds":2,"reference_run_available":False,
        "claim":"original scenario compatibility and native coast/save/resume; upstream trajectory parity unverified"}
(runtime/"insertion-verification.json").write_text(json.dumps(report,indent=2)+"\n")
print(report["claim"])
from NativeDemoLaunch import run_documented_demo
run_documented_demo(exe,runtime,"insertion")
