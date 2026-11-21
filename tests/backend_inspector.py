"""Read-only inspector CLI regression tests; no EVE launch or target input."""
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import ctypes as c
from ctypes import wintypes as w
import os

exe = pathlib.Path(sys.argv[1]).resolve()


def run(args, code=0, binary=exe):
    result = subprocess.run([str(binary), *map(str, args)], capture_output=True,
                            encoding='utf-8', errors='strict', timeout=8)
    assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
    return result


def read(path):
    data = json.loads(path.read_text(encoding='utf-8'))
    assert data['schema_version'] == 1 and data['pointer_bits'] == 64
    for target in data['targets']:
        assert set(target['mechanisms'].values()) == {'UNKNOWN'}
        assert all(item['state'] in ('foreground', 'background', 'UNKNOWN') for item in target['focus'])
    return data


run(['--help'])
for args in ([], ['0'], ['-1'], ['+1'], ['4294967296'], ['1junk'],
             ['self', '99'], ['self', '600001'], ['self', '100', '200'],
             ['self', '--json'], ['self', '--bogus'], ['--list-eve', '100'],
             ['self', '100', '--json', 'a', '--json', 'b']):
    run(args, 2)

with tempfile.TemporaryDirectory(prefix='hvi_inspector_') as directory:
    directory = pathlib.Path(directory)
    path = directory / 'наблюдение с пробелами.json'
    # Unicode executable path also exercises UTF-8 module names/path escaping.
    copy = directory / 'инспектор.exe'
    shutil.copy2(exe, copy)
    for dll in exe.parent.glob('*.dll'):
        if dll.name.startswith(('libgcc_', 'libstdc++', 'libwinpthread')):
            shutil.copy2(dll, directory / dll.name)
    run(['self', '1100', '--json', path], binary=copy)
    data = read(path)
    target, = data['targets']
    assert target['complete_snapshots'] >= 2 and not target['errors']
    assert any(item['path'] == str(copy) for item in target['modules'])
    assert len({item['path'] for item in target['modules']}) == len(target['modules'])
    assert target['focus'] and 1100 <= data['elapsed_ms'] < 4000
    assert data['diagnostic_revision'] == 2
    assert sum(target['focus_samples'].values()) > 0
    assert target['window_samples'] > 0 and target['window_changes']

    # Inspect a real owned top-level window without sending it any messages.
    user = c.WinDLL('user32', use_last_error=True)
    user.CreateWindowExW.argtypes = [w.DWORD, w.LPCWSTR, w.LPCWSTR, w.DWORD,
                                    c.c_int, c.c_int, c.c_int, c.c_int,
                                    w.HWND, w.HMENU, w.HINSTANCE, c.c_void_p]
    user.CreateWindowExW.restype = w.HWND
    user.DestroyWindow.argtypes = [w.HWND]
    window = user.CreateWindowExW(0, 'STATIC', 'PRIVATE TITLE NOT TO BE LOGGED',
                                 0, 0, 0, 100, 100, None, None, None, None)
    assert window, c.get_last_error()
    try:
        run([os.getpid(), '100', '--json', path])
        data = read(path)
        target, = data['targets']
        items = target['window_changes'][0]['snapshot']['items']
        item, = [item for item in items if item['hwnd'] == str(window)]
        assert item['class'] == 'Static' and item['thread_id'] > 0
        assert item['gui_thread_status'] == 'CONFIRMED'
        assert item['visible'] is False and item['minimized'] is False
        assert 'PRIVATE TITLE' not in path.read_text(encoding='utf-8')
    finally:
        user.DestroyWindow(window)

    run(['--list-eve', '--json', path])
    data = read(path)
    assert data['mode'] == 'list-eve' and not data['targets']
    assert all(item['identity'] == 'UNKNOWN' for item in data['candidates'])

    # A PID belonging to an already reaped owned child, not a guessed live PID.
    child = subprocess.Popen([sys.executable, '-c', 'pass'])
    child.wait(timeout=5)
    run([child.pid, '100', '--json', path], 1)
    target, = read(path)['targets']
    assert target['module_enumeration'] == 'UNKNOWN' and target['errors']
    run(['self', '100', '--json', directory / 'missing' / 'file.json'], 3)

    # Deterministic discovery without requiring a live game: owned inspector copy
    # named exefile.exe. A matching name must never assert verified EVE identity.
    candidate = directory / 'exefile.exe'
    shutil.copy2(exe, candidate)
    child = subprocess.Popen([str(candidate), 'self', '10000'], stdout=subprocess.DEVNULL)
    try:
        run(['--eve', '100', '--json', path])
        data = read(path)
        assert any(item['pid'] == child.pid and item['identity'] == 'UNKNOWN'
                   and item['role'] == 'client_candidate' for item in data['candidates'])
        assert any(item['pid'] == child.pid for item in data['targets'])
    finally:
        child.terminate()
        child.wait(timeout=5)

print('PASS inspector arguments, UTF-8 JSON, modules, focus, discovery and failure paths')