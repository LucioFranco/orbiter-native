"""Fresh-process NASSP save/load/continue regression through the native CLI."""
from pathlib import Path
import math
import re
import subprocess
import sys
import uuid

executable=Path(sys.argv[1]).resolve()
runtime=Path(sys.argv[2]).resolve()
prefix="NativeResumeTest_"+uuid.uuid4().hex
names=[prefix+suffix for suffix in ("_saved","_loaded","_continued")]
paths=[runtime / "Scenarios" / (name+".scn") for name in names]
def run(source,steps,destination):
    result=subprocess.run([str(executable),"--root",str(runtime),"--scenario",source,
                           "--steps",str(steps),"--save",destination],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    if result.returncode:
        raise RuntimeError(result.stdout[-12000:])
def block(text,name):
    return text.split(name+"_BEGIN\n",1)[1].split(name+"_END",1)[0]
def scalar(text,name):
    return float(re.search(r"^\s*"+name+r"\s+([^\n]+)",text,re.M).group(1))
def agc_state(text):
    # NASSP deliberately excludes CycleCounter on load until CDU queues are serialized.
    return [line for line in block(text,"AGC").splitlines() if not line.strip().startswith("CYCLECOUNTER ")]
try:
    run("NasspInsertion",120,names[0])
    saved=paths[0].read_text()
    run(names[0],0,names[1])
    loaded=paths[1].read_text()
    assert agc_state(saved)==agc_state(loaded),"AGC saved state changes during reload"
    assert block(saved,"DSKY")==block(loaded,"DSKY"),"DSKY state changes during reload"
    assert block(saved,"DSKY2")==block(loaded,"DSKY2"),"LEB DSKY state changes during reload"
    assert abs(scalar(saved,"MISSNTIME")-scalar(loaded,"MISSNTIME"))<1e-9,"mission clock lost during reload"
    for field in ("STAGE","CREWEQUIPMENTSTATE","VIEWPOS"):
        assert scalar(saved,field)==scalar(loaded,field),field+" lost during reload"
    assert abs(scalar(saved,"Date MJD")-scalar(loaded,"Date MJD"))<1e-9,"saved epoch lost"
    for field in ("RPOS","RVEL"):
        def vector(text):return [float(x) for x in re.search(r"^\s*"+field+r"\s+([^\n]+)",text,re.M).group(1).split()]
        assert math.dist(vector(saved),vector(loaded))<1.0,field+" not preserved to scenario precision"
    run(names[1],120,names[2])
    continued=paths[2].read_text()
    assert abs(scalar(continued,"MISSNTIME")-scalar(loaded,"MISSNTIME")-2)<1e-6,"mission clock did not resume"
    assert abs(scalar(continued,"Date MJD")-scalar(loaded,"Date MJD")-2/86400)<1e-9,"world clock did not resume"
    assert scalar(block(continued,"AGC"),"CYCLECOUNTER")>0,"AGC does not execute after resume"
    assert agc_state(continued)!=agc_state(loaded),"AGC state does not advance"
    print("Fresh-process NASSP save/reload preserves AGC memory, DSKYs, orbit and mission state; resumed AGC executes")
finally:
    for path in paths:
        path.unlink(missing_ok=True)
