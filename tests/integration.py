import pathlib, subprocess, ctypes, struct, time, os
root=pathlib.Path(__file__).resolve().parents[1]
os.environ['PATH']=str(root/'tools/installed/mingw64/bin')+os.pathsep+os.environ['PATH']
reports=root/'analysis/reports'
def start(id):
    return subprocess.Popen([str(root/'build/receiver_a.exe'),str(id),str(reports/f'recovery-{id}.log')],creationflags=0x08000000)
def connect(id):
    for _ in range(100):
        try: return open(fr'\\.\pipe\HVI.experimental.{id}','r+b',buffering=0)
        except OSError: time.sleep(.05)
    raise RuntimeError('pipe unavailable')
def send(f,id,seq,kind,a=0,b=0):
    f.write(struct.pack('<5I2i',0x31495648,1,id,seq,kind,a,b))
    assert f.read(4)==struct.pack('<I',seq)
processes=[]
try:
    p=start(31);q=start(32);processes.extend([p,q])
    foreground=ctypes.windll.user32.GetForegroundWindow()
    with connect(31) as a, connect(32) as b:
        send(a,31,1,1,65,1);send(b,32,1,1,66,1)
        p.kill();p.wait(timeout=5)
        send(b,32,2,6)
    time.sleep(.15)
    with connect(32) as b:
        send(b,32,1,6);send(b,32,2,7)
    q.wait(timeout=5)
    p=start(31);processes.append(p)
    with connect(31) as a: send(a,31,1,6);send(a,31,2,7)
    p.wait(timeout=5)
    text=(reports/'recovery-32.log').read_text()
    assert 'A=0 B=1' in text and 'disconnect reset' in text
    assert 'event 32 1 6 0 0 x=0 y=0 A=0 B=0' in text
    assert 'A=1' not in text
    assert 'A=0 B=0' in (reports/'recovery-31.log').read_text()
    result=f'PASS: peer termination isolation; reconnect reset; process restart; foreground unchanged={foreground==ctypes.windll.user32.GetForegroundWindow()}\n'
    (reports/'integration-result.txt').write_text(result);print(result)
finally:
    for p in processes:
        if p.poll() is None: p.kill();p.wait()