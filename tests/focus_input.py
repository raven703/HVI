import ctypes,pathlib,subprocess,time,struct,os,json
root=pathlib.Path(__file__).resolve().parents[1]
os.environ['PATH']=str(root/'tools/installed/mingw64/bin')+os.pathsep+os.environ['PATH']
user=ctypes.windll.user32
user.CreateWindowExW.restype=ctypes.c_void_p
user.CreateWindowExW.argtypes=[ctypes.c_ulong,ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p]
user.SetForegroundWindow.argtypes=[ctypes.c_void_p]
user.DestroyWindow.argtypes=[ctypes.c_void_p]
user.ShowWindow.argtypes=[ctypes.c_void_p,ctypes.c_int]
sink=user.CreateWindowExW(0,'STATIC','HVI focus sink',0x00CF0000,400,0,300,100,None,None,None,None)
assert sink
user.ShowWindow(sink,5)
log=root/'analysis/reports/focus-probe.log'
p=subprocess.Popen([str(root/'build/api_probe.exe'),'61',str(log),'--focus-test'],creationflags=0x08000000)
try:
    user.AllowSetForegroundWindow(p.pid)
    f=None
    for _ in range(100):
        try:f=open(r'\\.\pipe\HVI.experimental.61','r+b',buffering=0);break
        except OSError:time.sleep(.05)
    assert f
    with f:
        for seq,(kind,a,b) in enumerate([(1,65,1),(9,1,0),(6,0,0),(9,0,0),(6,0,0),(1,65,0),(7,0,0)],1):
            f.write(struct.pack('<5I2i',0x31495648,1,61,seq,kind,a,b));assert f.read(4)==struct.pack('<I',seq);time.sleep(.1)
    assert p.wait(timeout=5)==0
    samples=[s for s in log.read_text().splitlines() if s.startswith('sample')]
    assert all('consistent=1' in s for s in samples)
    changed='background=0' in samples[2] and 'background=1' in samples[4]
    assert 'A=1' in samples[2] and 'A=1' in samples[4] and 'A=0' in samples[5]
    result={'api_consistency':True,'held_across_focus_requests':True,'actual_foreground_gain_and_loss':changed,'status':'PASS' if changed else 'INCONCLUSIVE_FOREGROUND_POLICY'}
    (root/'analysis/reports/focus-result.json').write_text(json.dumps(result,indent=2));print(result)
finally:
    if p.poll() is None:p.kill();p.wait()
    user.DestroyWindow(sink)