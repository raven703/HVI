"""Interactive end-to-end test: controller -> SDL API -> actual object results.
Observation files are output only; never contain input commands.
"""
import ctypes as c
from ctypes import wintypes as w
import json, os, pathlib, subprocess, time, tempfile
u=c.WinDLL('user32',use_last_error=True)
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=c.c_ssize_t
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.SetForegroundWindow.argtypes=[w.HWND]
u.GetForegroundWindow.restype=w.HWND
root=pathlib.Path(__file__).resolve().parents[1];build=root/'build'
children=[];results=[]
def check(ok,label):
    results.append((bool(ok),label));print(('PASS ' if ok else 'FAIL ')+label,flush=True)
def read(path):
    for _ in range(50):
        try:return json.loads(path.read_text())
        except (OSError,ValueError):time.sleep(.02)
    raise RuntimeError('observation missing '+str(path))
def launch(args,env=None):
    p=subprocess.Popen([str(build/args[0]),*args[1:]],cwd=build,env=env);children.append(p);return p
def windows(pid):
    found=[]
    @c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
    def visit(h,_):
        n=w.DWORD();u.GetWindowThreadProcessId(h,c.byref(n))
        if n.value==pid and u.IsWindowVisible(h):found.append(h)
        return True
    u.EnumWindows(visit,0);return found
def wait_window(p):
    for _ in range(100):
        found=windows(p.pid)
        if found:return found[0]
        if p.poll() is not None:raise RuntimeError('process exited '+str(p.returncode))
        time.sleep(.05)
    raise RuntimeError('window missing')
def close(p):
    for h in windows(p.pid):u.PostMessageW(h,0x10,0,0)
    try:p.wait(4)
    except subprocess.TimeoutExpired:p.terminate();p.wait();raise RuntimeError('hung child')
def keyphysical(vk,down):u.keybd_event(vk,u.MapVirtualKeyW(vk,0),0 if down else 2,0)
def foreground(hwnd):
    # Activation is not an input-command channel; verify Windows really grants it.
    time.sleep(.25)
    for _ in range(10):
        keyphysical(0x12,True);keyphysical(0x12,False);u.SetForegroundWindow(hwnd);time.sleep(.1)
        if u.GetForegroundWindow()==hwnd:return
    raise RuntimeError('Windows refused foreground activation')
try:
    if u.FindWindowW('HviController',None):raise RuntimeError('Close running HVI demos first')
    with tempfile.TemporaryDirectory(prefix='hvi_sdl_') as td:
        td=pathlib.Path(td)
        # Baseline with instrumentation only, no Runtime initialized.
        env=os.environ.copy();env['SDL_DYNAMIC_API']=str(build/'hvi_compat.dll');env['HVI_TRACE_ONLY']='1';env['HVI_COMPAT_TRACE']=str(td/'native_trace.json')
        baseline=launch(['sdl_input_test.exe','--observe',str(td/'native.json')],env);hw=wait_window(baseline);foreground(hw);time.sleep(.3);before=read(td/'native.json');keyphysical(ord('W'),True);time.sleep(.35);keyphysical(ord('W'),False);time.sleep(.15);after=read(td/'native.json');check(after['y']<before['y']-10,'native baseline physical W moves square');close(baseline)
        env['HVI_COMPAT_TRACE']=str(td/'relative_trace.json')
        relativeApp=launch(['sdl_input_test.exe','--relative','--observe',str(td/'relative.json')],env);rh=wait_window(relativeApp);foreground(rh);time.sleep(.3);close(relativeApp)
        paths=[];apps=[];handles=[]
        for i in (1,2):
            path=td/f'{i}.json';paths.append(path);env=os.environ.copy();env['SDL_DYNAMIC_API']=str(build/'hvi_compat.dll');env['HVI_COMPAT_SESSION']=str(i);env['HVI_COMPAT_TRACE']=str(td/f'trace{i}.json')
            p=launch(['sdl_input_test.exe','--title',f'SDL Test {i}','--observe',str(path)],env);apps.append(p);handles.append(wait_window(p))
        control=launch(['hvi_gui_controller.exe']);hw=wait_window(control);u.SetForegroundWindow(hw);time.sleep(.4)
        def target(i):
            box=u.GetDlgItem(hw,100);u.SendMessageW(box,0x14e,i,0);u.SendMessageW(hw,0x111,100|(1<<16),box)
        def click(id):u.SendMessageW(hw,0x111,id,u.GetDlgItem(hw,id));time.sleep(.15)
        def key(index,down):u.SendMessageW(u.GetDlgItem(hw,110),0x14e,index,0);click(103 if down else 104)
        def observe():return [read(p) for p in paths]
        target(0);key(0,True);time.sleep(.3);a,b=observe();check(a['y']<220 and b['y']==240,'compat B: W moves only A square');check(not a['focus'] and not b['focus'],'both SDL windows background')
        target(1);key(2,True);key(3,True);time.sleep(.3);aa,bb=observe();check(aa['y']<a['y'] and bb['y']>b['y'] and bb['x']>b['x'],'independent simultaneous object movement');click(102)
        # Send mouse through real controller pad messages, no object-state writes.
        u.FindWindowExW.argtypes=[w.HWND,w.HWND,w.LPCWSTR,w.LPCWSTR];u.FindWindowExW.restype=w.HWND
        pad=u.FindWindowExW(hw,None,'HviMousePad',None)
        point=w.POINT();u.GetCursorPos(c.byref(point));saved=(point.x,point.y)
        target(0);origin=w.POINT(30,35);u.ClientToScreen(pad,c.byref(origin));u.SetCursorPos(origin.x,origin.y);time.sleep(.15);u.SetCursorPos(origin.x+20,origin.y+15);time.sleep(.25);a,b=observe();check(a['aimX']!=b['aimX'] and a['aimY']!=b['aimY'],'independent crosshair reaction');u.GetCursorPos(c.byref(point));expected=(point.x,point.y);click(107);u.GetCursorPos(c.byref(point));check((point.x,point.y)==expected,'virtual mouse does not move Windows cursor');u.SetCursorPos(*saved)
        target(0);click(300);a,b=observe();check(a['color']==1 and b['color']==0,'LMB changes only A color')
        oldA,oldB=observe();target(1);click(107);a,b=observe();check(b['scale']>oldB['scale'] and a['scale']==oldA['scale'],'wheel changes only B scale')
        target(2);key(1,True);before=observe();time.sleep(.3);after=observe();check(all(after[i]['x']<before[i]['x'] for i in (0,1)),'BOTH moves both squares');click(102)
        target(3);before=observe();key(0,True);time.sleep(.25);after=observe();check(all(after[i]['y']==before[i]['y'] for i in (0,1)),'NONE does not move objects');click(102)
        target(0);key(0,True);click(109);time.sleep(.25);a,b=observe();check(not a['w'],'disconnect clears held key in SDL');key(0,False);key(3,True);time.sleep(.2);check(read(paths[0])['d']==1,'reconnect restores SDL input');click(102)
        # Toggle compatibility off via native F12, then native physical W.
        foreground(handles[0]);time.sleep(.2);keyphysical(0x7b,True);time.sleep(.15);keyphysical(0x7b,False);time.sleep(.15);before=read(paths[0]);keyphysical(ord('W'),True);time.sleep(.3);keyphysical(ord('W'),False);time.sleep(.15);after=read(paths[0]);check(after['y']<before['y']-10,'disable restores native physical input')
        close(apps[0]);target(1);key(1,True);before=read(paths[1]);time.sleep(.25);after=read(paths[1]);check(after['x']<before['x'],'B survives A close');click(102)
        # Preserve actual evidence outside temp directory.
        report={'checks':results,'nativeTrace':read(td/'native_trace.json'),'nativeRelativeTrace':read(td/'relative_trace.json'),'compatTrace1':read(td/'trace1.json'),'compatTrace2':read(td/'trace2.json')}
        (build/'sdl_compatibility_result.json').write_text(json.dumps(report,indent=2))
finally:
    keyphysical(ord('W'),False);keyphysical(0x7b,False)
    for p in reversed(children):
        if p.poll() is None:close(p)
if not all(ok for ok,_ in results):raise SystemExit(1)