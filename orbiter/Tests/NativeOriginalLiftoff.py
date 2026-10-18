"""Unchanged ten-vessel Apollo 11 liftoff, real cockpit controls, save/reload gate."""
from pathlib import Path
import hashlib
import json
import math
import os
import re
import subprocess
import sys
exe,flight,graphics,runtime,source=(Path(p).resolve() for p in sys.argv[1:])
scenario="Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m"
assert (runtime/"Scenarios"/(scenario+".scn")).read_bytes()==source.read_bytes()
def vessels(text):
    block=text.split("BEGIN_SHIPS",1)[1].split("END_SHIPS",1)[0]
    return {m[0]:(m[1].replace("\\","/"),m[2]) for m in re.findall(r"^([^\s:]+):([^\n]+)\n(.*?)^END$",block,re.M|re.S)}
def field(text,key):return re.search(r"^\s*"+re.escape(key)+r"\s+(.+)$",text,re.M)[1]
def radius(text):return math.sqrt(sum(float(x)**2 for x in field(text,"RPOS").split()))
def run(name,command,env=None):
    result=subprocess.run(command,text=True,capture_output=True,timeout=120,env=env)
    (runtime/(name+".log")).write_text(result.stdout+result.stderr)
    assert result.returncode==0,(result.stdout+result.stderr)[-8000:]
    log=(runtime/"Orbiter.log").read_text(errors="replace")
    assert "ERROR:" not in log,log[-8000:]
def cli(scenario,steps,save):
    run(save,[str(exe),"--root",str(runtime),"--scenario",scenario,"--steps",str(steps),"--dt","0.016666666666666666","--save",save])
    return (runtime/"Scenarios"/(save+".scn")).read_text()
original=vessels(source.read_text());loaded=vessels(cli(scenario,0,"OriginalLiftoffLoaded"))
assert {k:v[0] for k,v in loaded.items()}=={k:v[0] for k,v in original.items()}
assert len(loaded)==10
assert abs(float(field(loaded["Eagle"][1],"MISSNTIME"))-float(field(original["Eagle"][1],"MISSNTIME")))<1e-9
env=dict(os.environ,ORBITER_LIFTOFF_CAPTURE=str(runtime/"original-liftoff.ppm"))
run("OriginalLiftoffFlight",[str(flight),str(graphics),str(runtime),"--original"],env)
ascent=vessels((runtime/"Scenarios/NativeLunarAscent.scn").read_text())
assert set(ascent)==set(original)|{"Eagle-DESCENTSTG"}
assert field(ascent["Eagle"][1],"CONFIGURATION")=="2"
reloaded=vessels(cli("NativeLunarAscent",0,"OriginalLiftoffReloaded"))
assert set(reloaded)==set(ascent)
def agc(text):
    state=text.split("AGC_BEGIN",1)[1].split("AGC_END",1)[0]
    return [line for line in state.splitlines() if not line.strip().startswith("CYCLECOUNTER ")]
assert agc(reloaded["Eagle"][1])==agc(ascent["Eagle"][1]),"LGC save state retained"
continued=vessels(cli("OriginalLiftoffReloaded",120,"OriginalLiftoffContinued"))
assert set(continued)==set(ascent)
assert radius(continued["Eagle"][1])>radius(reloaded["Eagle"][1])+10,"ascent continues after reload"
assert abs(float(field(continued["Eagle"][1],"MISSNTIME"))-float(field(reloaded["Eagle"][1],"MISSNTIME"))-2)<1e-5
before,after=reloaded["Eagle-DESCENTSTG"][1],continued["Eagle-DESCENTSTG"][1]
if "  RPOS " in before and "  RPOS " in after:assert abs(radius(after)-radius(before))<0.2
else:assert field(after,"STATUS")=="Landed Moon"
report={"source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),"scenario_modified":False,"original_vessels":sorted(original),"ascent_vessels":sorted(ascent),"resume_seconds":2,"reference_run_available":False,"claim":"original scenario compatibility, cockpit-commanded liftoff and fresh-process continuation; trajectory parity unverified"}
(runtime/"liftoff-verification.json").write_text(json.dumps(report,indent=2)+"\n")
print(report["claim"])
from NativeDemoLaunch import run_documented_demo
run_documented_demo(exe,runtime,"liftoff")
