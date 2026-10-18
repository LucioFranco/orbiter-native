"""Verify native LM/AGS save state in fresh processes, including 64-bit clocks."""
from pathlib import Path
import re
import subprocess
import sys
import uuid
executable,runtime=map(lambda s:Path(s).resolve(),sys.argv[1:])
name='NativeLMResume_'+uuid.uuid4().hex
source=runtime/'Scenarios/NativeLMSaved.scn'
resumed=runtime/'Scenarios'/f'{name}.scn'
def field(s,key):
    return re.search(r'^\s*'+key+r'\s+(\S+)',s,re.M)[1]
def aea(s):return s.split('AEA_START',1)[1].split('AEA_END',1)[0]
try:
    original=source.read_text()
    result=subprocess.run([str(executable),'--root',str(runtime),'--scenario','NativeLMSaved','--steps','0','--save',name],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    assert result.returncode==0,result.stdout
    restored=resumed.read_text()
    for key in ('CYCLECOUNTER','NEXT20MSSIGNAL'):
        expected=int(field(aea(original),key))
        assert expected>2**32,'fixture must exercise 64-bit AGS counters'
        assert int(field(aea(restored),key))==expected,f'{key} must round-trip without truncation'
    assert field(original,'VIEWPOS')==field(restored,'VIEWPOS')=='3'
    assert abs(float(field(original,'MISSNTIME'))-float(field(restored,'MISSNTIME')))<1e-5
    result=subprocess.run([str(executable),'--root',str(runtime),'--scenario',name,'--steps','30'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    assert result.returncode==0,result.stdout
    result=subprocess.run([str(executable),'--root',str(runtime),'--scenario','NativeLMStaged','--steps','30','--save',name],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    assert result.returncode==0,result.stdout
    staged=resumed.read_text()
    ships=re.findall(r'^([^\s:]+):ProjectApollo/([^\s]+)$',staged,re.M)
    assert ships==[('Eagle','LEM'),('Eagle-DESCENTSTG','Sat5LMDSC')],ships
    assert field(staged,'CONFIGURATION')=='2','Eagle stays in ascent configuration'
    assert field(staged.split('Eagle-DESCENTSTG:')[1],'STATE')=='2','descent stage retains deployed legs'
    result=subprocess.run([str(executable),'--root',str(runtime),'--scenario','NativeLMAscent','--steps','30','--save',name],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    assert result.returncode==0,result.stdout
    burning=resumed.read_text()
    aps=burning.split('APS_BEGIN')[1].split('APS_END')[0]
    assert field(aps,'THRUSTON')=='1' and field(aps,'ARMEDON')=='1'
    assert float(field(aps,'ACTUATORISOLATIONVALVE'))>0.99
    assert field(burning,'CONFIGURATION')=='2'
    print('Fresh reload resumes the active ascent burn')
    print('Fresh reload advances both separated LM stages without duplicating them')
    print('Fresh native LM reload preserves 64-bit AGS clocks, mission time and DSKY viewpoint, then resumes simulation')
finally:
    resumed.unlink(missing_ok=True)
