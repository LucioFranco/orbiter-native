"""Mac-only guided-descent diagnostic gate; simplified pilot sequence, no Windows parity claim."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import subprocess
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument("probe",type=Path);p.add_argument("orbiter",type=Path)
p.add_argument("runtime",type=Path);p.add_argument("source",type=Path)
p.add_argument("--duration",type=int,default=1750)
a=p.parse_args();probe,orbiter,runtime,source=(x.resolve() for x in (a.probe,a.orbiter,a.runtime,a.source))
scenario="Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m"
assert (runtime/"Scenarios"/(scenario+".scn")).read_bytes()==source.read_bytes(),"unchanged original scenario required"
started=time.monotonic()
cmd=[str(probe),str(runtime),str(a.duration),"--prepare-pdi","--scenario",scenario,
     "--radar-limit-ft","2000","--proceed-approach","--stop-at-contact"]
run=subprocess.run(cmd,text=True,capture_output=True,timeout=360)
log=run.stdout+run.stderr;(runtime/"NativeDescentProbe.log").write_text(log)
assert run.returncode==0,log[-6000:]
contact=re.search(r"FIRST_CONTACT sim_elapsed=(\S+) precontact_vertical_speed=(\S+) precontact_groundspeed=(\S+) soft_contact=(\d)",log)
assert contact and contact[4]=="1","first contact must be soft, not an impact followed by ground settling"
assert "RADAR_ACCEPTED" in log and "Approach PRO applied" in log and "PROG 65" in log,"original LGC radar/approach/P65 path required"
assert "Original engine STOP applied" in log,"original shutdown input required"
loaded=(runtime/"Scenarios/NativeDescentCheckpoint.scn").read_text()
def vessels(text):
    ships=text.split("BEGIN_SHIPS",1)[1].split("END_SHIPS",1)[0]
    return {name:(cls.replace("\\","/"),state) for name,cls,state in re.findall(r"^([^\s:]+):([^\n]+)\n(.*?)^END$",ships,re.M|re.S)}
def field(text,key):
    return re.search(r"^\s*"+re.escape(key)+r"\s+([^\n]+)",text,re.M)[1]
def agc(state):
    block=state.split("AGC_BEGIN",1)[1].split("AGC_END",1)[0]
    return [line for line in block.splitlines() if not line.strip().startswith("CYCLECOUNTER ")]
original=vessels(source.read_text());landed=vessels(loaded)
assert {n:s[0] for n,s in original.items()}=={n:s[0] for n,s in landed.items()},"every original vessel survives descent"
eagle=landed["Eagle"][1]
assert field(eagle,"CDRManualEngineStop").startswith("1 "),"original engine-stop state retained"
assert field(eagle,"EngineArmSwitch").startswith("0 "),"descent stage retained"
assert field(eagle,"PROG")=="65","original LGC P65 retained"
last=re.findall(r"elapsed=.*?altitude=(\S+) groundspeed=(\S+) dps=(\S+) contact=(\d)",log)[-1]
assert float(last[1])<0.1 and float(last[2])<0.001 and last[3]=="1","settled contact with DPS off required"
def fresh(name,steps,save):
    result=subprocess.run([str(orbiter),"--root",str(runtime),"--scenario",name,"--steps",str(steps),"--dt","0.016666666666666666","--save",save],text=True,capture_output=True,timeout=120)
    (runtime/(save+".log")).write_text(result.stdout+result.stderr)
    assert result.returncode==0,(result.stdout+result.stderr)[-6000:]
    assert "ERROR:" not in (runtime/"Orbiter.log").read_text(errors="replace")
    return (runtime/"Scenarios"/(save+".scn")).read_text()
reloaded=fresh("NativeDescentCheckpoint",0,"NativeDescentReloaded")
again=vessels(reloaded)
assert set(again)==set(landed) and agc(again["Eagle"][1])==agc(eagle),"fresh process retains vessels and LGC memory/channels"
# Orbiter can serialize a stable contacting vessel as Orbiting while it remains
# active. Check actual contact/velocity/thrust after reload instead of that label.
result=subprocess.run([str(probe),str(runtime),"10","--scenario","NativeDescentReloaded","--save","NativeDescentContinued"],text=True,capture_output=True,timeout=120)
resume_log=result.stdout+result.stderr;(runtime/"NativeDescentContinued.log").write_text(resume_log)
assert result.returncode==0,resume_log[-6000:]
resumed=(runtime/"Scenarios/NativeDescentContinued.scn").read_text();continued=vessels(resumed)
resume_last=re.findall(r"elapsed=.*?altitude=(\S+) groundspeed=(\S+) dps=(\S+) contact=(\d)",resume_log)[-1]
assert set(continued)==set(landed),"fresh continuation retains all vessels"
assert resume_last[3]=="1" and float(resume_last[1])<0.1 and float(resume_last[2])<0.001,"fresh process retains physical contact with DPS off"
assert abs(float(resume_last[0])-float(last[0]))<0.2,"fresh process remains settled at the landing height"
assert abs(float(field(continued["Eagle"][1],"MISSNTIME"))-float(field(eagle,"MISSNTIME"))-10)<1e-4,"fresh mission clock advances ten seconds"
report={"scenario":scenario+".scn","source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),
    "scenario_modified":False,"command":cmd,"duration_seconds":a.duration,"wall_seconds":time.monotonic()-started,
    "first_contact_sim_elapsed":float(contact[1]),"precontact_vertical_speed_mps":float(contact[2]),
    "precontact_groundspeed_mps":float(contact[3]),"radar_limit_ft":2000,"final_altitude_m":float(last[0]),
    "final_groundspeed_mps":float(last[1]),"final_dps_level":float(last[2]),"vessels":sorted(landed),
    "fresh_resume_seconds":10,"fresh_ground_contact":True,"fresh_groundspeed_mps":float(resume_last[1]),"fresh_dps_level":float(resume_last[2]),"lgc_memory_channels_retained":True,"windows_comparison":False,
    "limits":["Simplified pilot sequence accepts initial attitude early and omits the checklist yaw/manual-landing exercise",
              "2000-foot radar acceptance is an explicit diagnostic bound, not an independently certified operational limit",
              "Soft contact/engine stop/save/resume establish Mac execution, not Windows trajectory or historical landing-site parity"]}
(runtime/"descent-verification.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
