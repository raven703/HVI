"""Real API call sites in two self-instrumented test executables; not game injection."""
import pathlib, subprocess, struct, time, os
root=pathlib.Path(__file__).resolve().parents[1]
os.environ['PATH']=str(root/'tools/installed/mingw64/bin')+os.pathsep+os.environ['PATH']
reports=root/'analysis/reports'
processes=[]
def connect(id):
    for _ in range(100):
        try:return open(fr'\\.\pipe\HVI.experimental.{id}','r+b',buffering=0)
        except OSError:time.sleep(.05)
    raise RuntimeError(f'probe {id} unavailable')
def send(f,id,seq,kind,a=0,b=0):
    f.write(struct.pack('<5I2i',0x31495648,1,id,seq,kind,a,b))
    assert f.read(4)==struct.pack('<I',seq),'missing application/API consistency acknowledgment'
try:
    for id in (41,42):
        processes.append(subprocess.Popen([str(root/'build/api_probe.exe'),str(id),str(reports/f'api-probe-{id}.log')],creationflags=0x08000000))
    with connect(41) as a,connect(42) as b:
        send(a,41,1,1,65,1);send(b,42,1,1,66,1)
        time.sleep(.1)
        send(a,41,2,6);send(b,42,2,6)
        send(a,41,3,1,65,0);send(b,42,3,1,66,0)
        send(a,41,4,7);send(b,42,4,7)
    for p in processes:assert p.wait(timeout=5)==0
    for id,held in ((41,'A=1 B=0'),(42,'A=0 B=1')):
        text=(reports/f'api-probe-{id}.log').read_text()
        assert held in text and 'seq=2 '+held in text
        assert 'seq=3 A=0 B=0' in text
        samples=[l for l in text.splitlines() if l.startswith('sample')]
        assert len(samples)==4 and all('background=1 consistent=1' in l for l in samples)
    result='PASS: two background GUI probes; self-IAT GetKeyboardState/GetAsyncKeyState and per-object DirectInput GetDeviceState agree; independent A/B hold and release. Native API baselines logged separately. No claim of native OS-state isolation.\n'
    (reports/'api-isolation-result.txt').write_text(result);print(result)
finally:
    for p in processes:
        if p.poll() is None:p.kill();p.wait()