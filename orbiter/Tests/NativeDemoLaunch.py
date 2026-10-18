"""Exercise the documented graphical CLI in bounded automated processes.

The appended step limits cover Main's window/VC/plugin/render/save pipeline.
Normal user window close and physical inputs remain separate live CUA checks.
"""
from pathlib import Path
import json
import os
import re
import subprocess
import sys

def run_documented_demo(executable,runtime,mission):
    root=Path(__file__).resolve().parents[1];build=executable.parent
    provenance=json.loads((runtime/"mission-source.json").read_text())["data_provenance"]
    command=[sys.executable,str(root/"tools/nassp_demo.py"),"stage","--build",str(build),
        "--nassp-data-root",provenance["data_root"],"--profile",provenance["profile"],
        "--mission",mission,"--output",str(runtime)]
    if provenance["orbiter_textures"]:command += ["--orbiter-textures",provenance["orbiter_textures"]]
    stage=subprocess.run(command,cwd=root,text=True,capture_output=True,timeout=120)
    assert stage.returncode==0,stage.stderr
    report=json.loads(stage.stdout);launch=report["launch_argv"]
    for flag in ("--graphics","--module","--window","--vc","--laptop-controls","--save"):
        assert flag in launch,"documented graphical flag missing: "+flag
    save=launch[launch.index("--save")+1]
    def run(argv,label,steps):
        actual=[*argv,"--steps",str(steps),"--profile"]
        result=subprocess.run(actual,cwd=root,text=True,capture_output=True,timeout=120)
        (runtime/(label+".log")).write_text(result.stdout+result.stderr)
        assert result.returncode==0,(result.stdout+result.stderr)[-8000:]
        assert f"Completed {steps} windowed steps" in result.stdout,result.stdout
        if steps>10:assert "samples=2" in result.stdout,"actual Main rendering/profile loop did not execute"
        assert "ERROR:" not in (runtime/"Orbiter.log").read_text(errors="replace")
        return (runtime/"Scenarios"/(argv[argv.index("--save")+1]+".scn")).read_text(),actual
    saved,first=run(launch,"DocumentedLaunch",12)
    reopened=list(launch);reopened[reopened.index("--scenario")+1]=save
    reopened[reopened.index("--save")+1]=save+"Reloaded"
    loaded,second=run(reopened,"DocumentedReopen",0)
    def ships(text):
        block=text.split("BEGIN_SHIPS",1)[1].split("END_SHIPS",1)[0]
        return {name:(cls,state) for name,cls,state in re.findall(r"^([^\s:]+):([^\n]+)\n(.*?)^END$",block,re.M|re.S)}
    before,after=ships(saved),ships(loaded)
    assert {n:s[0] for n,s in before.items()}=={n:s[0] for n,s in after.items()},"graphical reopen retains all vessels/classes"
    craft="Columbia" if mission=="insertion" else "Eagle"
    def field(text,key):return re.search(r"^\s*"+re.escape(key)+r"\s+([^\n]+)",text,re.M)[1]
    def agc(text):return [line for line in text.split("AGC_BEGIN",1)[1].split("AGC_END",1)[0].splitlines() if not line.strip().startswith("CYCLECOUNTER ")]
    assert agc(before[craft][1])==agc(after[craft][1]),"graphical reopen retains LGC/AGC memory and channels"
    assert field(before[craft][1],"MISSNTIME")==field(after[craft][1],"MISSNTIME"),"graphical reopen retains mission clock"
    resumed=list(reopened);resumed[resumed.index("--save")+1]=save+"Continued"
    advanced,third=run(resumed,"DocumentedContinue",12)
    assert abs(float(field(ships(advanced)[craft][1],"MISSNTIME"))-float(field(after[craft][1],"MISSNTIME"))-.2)<1e-5
    result={"documented_launch_argv":launch,"actual_bounded_commands":[first,second,third],
        "window_vc_graphics_addon_render_save_reopen_passed":True,"fresh_memory_channels_clock_retained":True,
        "continued_windowed_seconds":.2,"runtime_provenance":report["runtime_provenance"],
        "normal_window_close_tested":False,"human_cua_or_physical_controller_tested":False}
    (runtime/"graphical-launch-verification.json").write_text(json.dumps(result,indent=2)+"\n")
    print("Documented graphical launch/save/fresh reopen/continue passed; normal user window close remains live CUA")
    return result
