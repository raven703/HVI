"""Owned official SDL test: application-produced logs are the delivery oracle."""
import ctypes as c
from ctypes import wintypes as w
import json, os, pathlib, struct, subprocess, time

root = pathlib.Path(__file__).resolve().parents[1]
build = root / 'build'
u = c.WinDLL('user32', use_last_error=True)
k = c.WinDLL('kernel32', use_last_error=True)
k.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
k.CreateFileW.restype = w.HANDLE
k.WriteFile.argtypes = [w.HANDLE,c.c_void_p,w.DWORD,c.POINTER(w.DWORD),c.c_void_p]
k.ReadFile.argtypes = k.WriteFile.argtypes
k.CloseHandle.argtypes = [w.HANDLE]
u.GetForegroundWindow.restype = w.HWND
u.GetWindowThreadProcessId.argtypes = [w.HWND,c.POINTER(w.DWORD)]
u.SetForegroundWindow.argtypes = [w.HWND]
u.PostMessageW.argtypes = [w.HWND,w.UINT,w.WPARAM,w.LPARAM]
children, files, pipes, checks = [], [], [], []
def check(ok, label):
    checks.append([bool(ok), label])
    print(('PASS ' if ok else 'FAIL ') + label, flush=True)
    if not ok:
        raise RuntimeError(label)
def windows(pid):
    result = []
    @c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
    def visit(h, _):
        owner = w.DWORD()
        u.GetWindowThreadProcessId(h,c.byref(owner))
        if owner.value == pid and u.IsWindowVisible(h):
            result.append(h)
        return True
    u.EnumWindows(visit,0)
    return result
def launch(name, args=(), env=None, log=None):
    f = open(log, 'wb') if log else subprocess.DEVNULL
    if log:
        files.append(f)
    p = subprocess.Popen([str(build/name),*args],cwd=build,env=env,stdout=f,stderr=f)
    children.append(p)
    for _ in range(100):
        hs = windows(p.pid)
        if hs:
            return p, hs[0]
        if p.poll() is not None:
            raise RuntimeError(f'{name} exited {p.returncode}')
        time.sleep(.05)
    raise RuntimeError('No window: '+name)
def lines(i):
    return (build/f'second_sdl_{i}.log').read_text(errors='replace').count('adding line')
def wait_lines(expected):
    for _ in range(100):
        if [lines(0),lines(1)] == expected:
            time.sleep(.1)
            break
        time.sleep(.05)
    check([lines(0),lines(1)] == expected, f'application line counts {expected}')
try:
    apps = []
    for i in range(2):
        env = os.environ.copy()
        env['SDL_DYNAMIC_API'] = str(build/'hvi_compat.dll')
        env['HVI_COMPAT_SESSION'] = str(94+i)
        apps.append(launch('sdl_official_intersections.exe', ['--renderer','software','0'],env,build/f'second_sdl_{i}.log'))
    control, hwnd = launch('hvi_gui_controller.exe')
    for _ in range(10):
        u.keybd_event(0x12,0,0,0);u.keybd_event(0x12,0,2,0)
        u.SetForegroundWindow(hwnd)
        time.sleep(.1)
        if u.GetForegroundWindow() == hwnd:
            break
    check(u.GetForegroundWindow() == hwnd, 'controller foreground')
    for p, _ in apps:
        check(all(u.GetForegroundWindow()!=h for h in windows(p.pid)), f'official app {p.pid} background')
    for i in range(2):
        h = k.CreateFileW(r'\\.\pipe\HVI.runtime.'+str(94+i),0xc0000000,0,None,3,0x00120000,None)
        check(h not in (None,c.c_void_p(-1).value), 'connect session '+str(94+i))
        pipes.append(h)
    seq = [0,0]
    def send(i,kind,a=0,b=0):
        seq[i] += 1
        packet = struct.pack('<IIII Iii',0x31495648,1,94+i,seq[i],kind,a,b)
        n = w.DWORD();ack = w.DWORD()
        if not k.WriteFile(pipes[i],packet,28,c.byref(n),None) or n.value!=28:
            raise RuntimeError('write '+str(c.get_last_error()))
        if not k.ReadFile(pipes[i],c.byref(ack),4,c.byref(n),None) or n.value!=4 or ack.value!=seq[i]:
            raise RuntimeError('ack '+str(c.get_last_error()))
    # No sleeps between transitions. Five independent keyboard actions per app.
    for i in range(2):
        for _ in range(5):
            send(i,1,ord('L'),1);send(i,1,ord('L'),0)
        wait_lines([5,0] if i==0 else [5,5])
    # Five right-button drags; distinct nonzero endpoints avoid app's zero-line filter.
    for i in range(2):
        for j in range(5):
            send(i,3,20+j*10,30);send(i,2,1,1)
            send(i,3,70+j*10,90);send(i,2,1,0)
        wait_lines([10,5] if i==0 else [10,10])
    for p, _ in apps:
        observation = subprocess.run([str(build/'input_backend_inspector.exe'),str(p.pid),'200'],cwd=build,capture_output=True,text=True,timeout=5)
        (build/f'inspector_official_{p.pid}.log').write_text(observation.stdout)
        check(observation.returncode==0, 'passive modules observable '+str(p.pid))
finally:
    for h in pipes:
        k.CloseHandle(h)
    for p in reversed(children):
        if p.poll() is None:
            for h in windows(p.pid):
                u.PostMessageW(h,0x10,0,0)
            try:
                p.wait(4)
            except subprocess.TimeoutExpired:
                p.terminate();p.wait()
    for f in files:
        f.close()
    (build/'second_sdl_result.json').write_text(json.dumps({'checks':checks},indent=2))