"""Check bounded headless runs and the unbounded interactive default."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time
executable=Path(sys.argv[1]).resolve()
fixture=Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix='orbiter-cli-') as directory:
    runtime=Path(directory)
    shutil.copytree(fixture,runtime,dirs_exist_ok=True)
    command=[str(executable),'--root',str(runtime),'--scenario','Orbit','--dt','0.0001']
    environment={**os.environ,'SDL_VIDEODRIVER':'dummy'}
    def stop(child):
        try:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    # A dummy SDL window can leave shutdown stuck on a hosted runner.
                    # Cleanup is not part of the running-step assertions above.
                    child.kill()
                    child.wait(timeout=10)
        finally:
            # A descendant retaining stdout should not hold test cleanup open.
            if child.stdout:child.stdout.close()
    def run(args):
        result=subprocess.run(command+args,env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=10)
        assert result.returncode==0,result.stdout
        return result.stdout
    assert 'Completed 600 headless steps' in run([])
    assert 'samples=2' in run(['--steps','12','--profile'])
    assert 'Profile' not in run(['--steps','3','--profile'])
    assert 'Completed 0 headless steps' in run(['--steps','0'])
    assert 'Completed 3 windowed steps' in run(['--window','--steps','3'])
    started=time.monotonic()
    paced=run(['--window','--dt','0.01','--steps','25','--realtime'])
    assert 'Completed 25 windowed steps; simulation time 0.25 s' in paced,paced
    assert 0.20<=time.monotonic()-started<5,'realtime CLI ignored wall-clock pacing'

    child=subprocess.Popen(command+['--window'],env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        try:
            output,_=child.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            assert child.poll() is None,'interactive process stopped unexpectedly'
        else:
            raise AssertionError('Interactive session exited without a close event: '+output)
    finally:
        stop(child)
    # Startup initialization is separate from the caller's running-step cap.
    # Idle frames must keep the one-step window alive until an explicit resume.
    (runtime/'Orbiter.cfg').write_text('StartPaused = TRUE\nFixedStep = 0.0001\n')
    child=subprocess.Popen(command+['--window','--steps','1'],env=environment,
        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        try:
            output,_=child.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            assert child.poll() is None,'paused startup consumed the running-step cap'
        else:
            raise AssertionError('Paused window exited before resume: '+output)
    finally:
        stop(child)
print('Headless/default pacing, running-step caps, persistent window and paused startup verified')
