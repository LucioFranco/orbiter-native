"""Exercise non-default saved NASSP viewpoints in fresh native processes."""
from pathlib import Path
import re
import subprocess
import sys
import uuid
executable,plugin,runtime=map(lambda p:Path(p).resolve(),sys.argv[1:])
source=(runtime/'Scenarios/NasspCSM.scn').read_text()
for saved,expected,placement in ((2,2,"early"),(11,11,"late"),(31,0,"early"),(4,4,"legacy")):
    name='NativeViews_'+uuid.uuid4().hex
    path=runtime/'Scenarios'/f'{name}.scn'
    resumed=runtime/'Scenarios'/f'{name}_saved.scn'
    try:
        changed,count=re.subn(r'(?m)^(\s*VIEWPOS)\s+\d+',lambda m:f'{m[1]} {saved}',source)
        assert count==1,'fixture must have one VIEWPOS'
        if placement in ("late","legacy"):
            changed=re.sub(r'(?m)^[ \t]*VIEWPOS[^\n]*\n','',changed)
            def mainstate(match):
                if placement=="late":return match[0]+f'\n  VIEWPOS {saved}'
                # Legacy MainState::viewpos occupies bits 23..27.
                value=(int(match[2]) & ~(31<<23)) | (saved<<23)
                return match[1]+str(value)
            changed,count=re.subn(r'(?m)^([ \t]*MAINSTATE[ \t]+)(-?\d+)',mainstate,changed)
            assert count==1,'fixture must have one MAINSTATE'
        path.write_text(changed)
        result=subprocess.run([str(executable),str(plugin),str(runtime),name,str(expected)]+([name+"_saved"] if placement=="early" and saved==2 else []),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        assert result.returncode==0,result.stdout
        if placement=="early" and saved==2:
            assert resumed.exists(),'native save must produce a scenario for the reload check'
            result=subprocess.run([str(executable),str(plugin),str(runtime),name+"_saved","11"],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
            assert result.returncode==0,result.stdout
            print('Native save/reload preserves the selected side-hatch viewpoint')
        print(f'{placement} saved seat {saved} restored as {expected}; all seat transitions verified')
    finally:
        path.unlink(missing_ok=True)
        resumed.unlink(missing_ok=True)
