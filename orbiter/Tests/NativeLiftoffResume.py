"""Verify a fresh process continues lunar ascent and leaves the descent stage behind."""
from pathlib import Path
import math
import re
import subprocess
import sys
import uuid
executable,runtime=(Path(s).resolve() for s in sys.argv[1:])
name='NativeLiftoffResume_'+uuid.uuid4().hex
saved=runtime/'Scenarios'/f'{name}.scn'
def block(text,name):return re.search(r'^'+re.escape(name)+r':.*?^END$',text,re.M|re.S)[0]
def field(text,key):return re.search(r'^\s*'+key+r'\s+(.+)$',text,re.M)[1]
def radius(text):return math.sqrt(sum(float(v)**2 for v in field(text,'RPOS').split()))
try:
    original=(runtime/'Scenarios/NativeLunarAscent.scn').read_text()
    result=subprocess.run([str(executable),'--root',str(runtime),'--scenario','NativeLunarAscent','--steps','120','--save',name],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
    assert result.returncode==0,result.stdout
    resumed=saved.read_text()
    before,after=block(original,'Eagle'),block(resumed,'Eagle')
    assert field(after,'CONFIGURATION')=='2'
    assert radius(after)>radius(before)+10,'ascent continues climbing after reload'
    assert 1.99<float(field(after,'MISSNTIME'))-float(field(before,'MISSNTIME'))<2.01
    old_descent,new_descent=block(original,'Eagle-DESCENTSTG'),block(resumed,'Eagle-DESCENTSTG')
    # A stationary descent stage can serialize as either inactive LANDED or
    # active contact dynamics; RPOS is present for the latter.
    if '  RPOS ' in old_descent and '  RPOS ' in new_descent:
        assert abs(radius(new_descent)-radius(old_descent))<0.2,'descent stage remains at surface height'
    else:
        assert field(new_descent,'STATUS')=='Landed Moon'
    assert len(re.findall(r'^[^\s:]+:ProjectApollo/',resumed,re.M))==2
    print('Fresh-process lunar ascent continues climbing with its descent stage left on the Moon')
finally:
    saved.unlink(missing_ok=True)
