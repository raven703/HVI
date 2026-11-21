"""Smoke the actual three-window launcher and close only its new children."""
import ctypes as c
from ctypes import wintypes as w
import pathlib
import subprocess
import time

build = pathlib.Path(__file__).resolve().parents[1] / 'build'
u = c.WinDLL('user32', use_last_error=True)
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.IsWindowVisible.argtypes = [w.HWND]
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]


def windows():
    result = {}
    @c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def visit(hwnd, _):
        text = c.create_unicode_buffer(256)
        u.GetWindowTextW(hwnd, text, 256)
        name = next((name for name in ('SDL Test A', 'SDL Test B', 'HVI Control Panel')
                     if text.value == name or text.value.startswith(name + ' | ')), None)
        if u.IsWindowVisible(hwnd) and name:
            pid = w.DWORD()
            u.GetWindowThreadProcessId(hwnd, c.byref(pid))
            result[name] = (hwnd, pid.value)
        return True
    u.EnumWindows(visit, 0)
    return result


assert not windows(), 'Close HVI demos first'
try:
    subprocess.run([str(build / 'hvi_sdl_demo.exe')], cwd=build, check=True, timeout=10)
    for _ in range(100):
        found = windows()
        if len(found) == 3:
            break
        time.sleep(.05)
    assert len(found) == 3, found
    assert len({pid for _, pid in found.values()}) == 3, found
    print('PASS actual SDL launcher: three visible windows, three independent PIDs', found)
finally:
    for hwnd, _ in windows().values():
        u.PostMessageW(hwnd, 0x10, 0, 0)
    for _ in range(100):
        if not windows():
            break
        time.sleep(.05)
    assert not windows(), 'Demo windows failed to close'