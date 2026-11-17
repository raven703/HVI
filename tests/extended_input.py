import pathlib,subprocess,struct,time,os,concurrent.futures
root=pathlib.Path(__file__).resolve().parents[1]
os.environ['PATH']=str(root/'tools/installed/mingw64/bin')+os.pathsep+os.environ['PATH']
reports=root/'analysis/reports'
def worker(id,key):
    log=reports/f'extended-{id}.log'
    p=subprocess.Popen([str(root/'build/api_probe.exe'),str(id),str(log)],creationflags=0x08000000)
    try:
        f=None
        for _ in range(100):
            try:f=open(fr'\\.\pipe\HVI.experimental.{id}','r+b',buffering=0);break
            except OSError:time.sleep(.05)
        assert f is not None
        commands=[(1,key,1),(1,160,1),(1,161,1),(1,160,0),(3,100,200),(4,-7,9),(8,120,0),(2,0,1),(6,0,0),(1,key,0),(1,161,0),(2,0,0),(5,0,0),(7,0,0)]
        with f:
            for seq,(kind,a,b) in enumerate(commands,1):
                f.write(struct.pack('<5I2i',0x31495648,1,id,seq,kind,a,b));assert f.read(4)==struct.pack('<I',seq)
        assert p.wait(timeout=5)==0
        text=log.read_text();samples=[l for l in text.splitlines() if l.startswith('sample')]
        assert len(samples)==len(commands) and all('consistent=1' in s and 'background=1' in s for s in samples)
        assert 'shift=1' in samples[3] and 'shift=0' in samples[10]
        assert 'x=93 y=209 wheel=120 button0=1' in samples[8]
        assert 'x=0 y=0 wheel=0 button0=0' in samples[12]
        assert 'buffered=1' in samples[0]
    finally:
        if p.poll() is None:p.kill();p.wait()
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(lambda x:worker(*x),[(51,65),(52,66)]))
result='PASS: parallel process commands; A/B isolation; left/right Shift aggregation; held state; absolute/relative mouse; wheel/button; reset; DirectInput buffered peek/drain. Focus remains background; actual focus transitions not asserted.\n'
(reports/'extended-input-result.txt').write_text(result);print(result)