"""Reload both native spacecraft in a fresh process and preserve cockpit ownership."""
from pathlib import Path
import re
import subprocess
import sys
import uuid

executable, runtime = (Path(s).resolve() for s in sys.argv[1:])
name = 'NativePairResume_' + uuid.uuid4().hex
saved = runtime / 'Scenarios' / (name + '.scn')
def ship(text, name):
    return re.search(r'^' + name + r':.*?^END$', text, re.M | re.S)[0]
def field(text, key):
    return re.search(r'^\s*' + key + r'\s+(\S+)', text, re.M)[1]
def run(scenario, steps, *extra):
    result = subprocess.run([str(executable), '--root', str(runtime), '--scenario', scenario,
                             '--steps', str(steps), *extra], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    assert result.returncode == 0, result.stdout
try:
    original = (runtime / 'Scenarios/NativePairSaved.scn').read_text()
    run('NativePairSaved', 0, '--save', name)
    restored = saved.read_text()
    assert field(restored.split('BEGIN_FOCUS')[1], 'Ship') == 'Eagle'
    for vessel, seat in [('Columbia', '2'), ('Eagle', '3')]:
        before, after = ship(original, vessel), ship(restored, vessel)
        assert field(before, 'VIEWPOS') == field(after, 'VIEWPOS') == seat
        assert abs(float(field(before, 'MISSNTIME')) - float(field(after, 'MISSNTIME'))) < 1e-5
        assert before.split('DSKY_BEGIN')[1].split('DSKY_END')[0] == after.split('DSKY_BEGIN')[1].split('DSKY_END')[0]
    run(name, 30)
    print('Fresh combined reload preserves focus, both viewpoints and independent DSKYs, then advances both vessels')
finally:
    saved.unlink(missing_ok=True)
