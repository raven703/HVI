"""Summarize measured phases; durations use last-observation-carried-forward."""
import collections
import hashlib
import json
import pathlib
import sys

source, output = map(pathlib.Path, sys.argv[1:])
raw = source.read_bytes()
data = json.loads(raw)
lines = ['# EVE manual phases — 2026-11-21', '',
         'Source: `' + str(source) + '`', '',
         'SHA256: `' + hashlib.sha256(raw).hexdigest() + '`', '',
         'UTC start: ' + data['started_utc'] + '; elapsed: ' + str(data['elapsed_ms']) + ' ms.', '',
         'Durations below are reconstructed between sampled changes, not exact event times.',
         'Foreground (50ms nominal) and GUI (250ms nominal) snapshots are not atomic.',
         'No input events or application responses are recorded by this file.', '']
for target in data['targets']:
    focus = target['focus']
    windows = target['window_changes']
    totals = collections.Counter()
    for index, event in enumerate(focus):
        end = focus[index + 1]['t_ms'] if index + 1 < len(focus) else data['elapsed_ms']
        totals[event['state']] += end - event['t_ms']
    assert sum(totals.values()) == data['elapsed_ms']
    lines += ['## PID ' + str(target['pid']), '',
              '- Complete module snapshots: ' + str(target['complete_snapshots']),
              '- Unique module paths: ' + str(len(target['modules'])),
              '- Focus sample counts: `' + json.dumps(target['focus_samples']) + '`',
              '- Reconstructed state durations (ms): `' + json.dumps(dict(totals)) + '`',
              '- Module errors: `' + json.dumps(target['errors']) + '`',
              '- Module first-seen events after start: ' + str(sum(m['first_seen_ms'] > 0 for m in target['modules'])),
              '', '### Main-window state changes', '',
              '| t (s) | Minimized | Active HWND | Focus HWND | Capture HWND |',
              '|---:|---|---|---|---|']
    previous = None
    for snapshot in windows:
        for window in snapshot['snapshot']['items']:
            if window['class'] != 'trinityWindow':
                continue
            state = tuple(window[k] for k in ('minimized', 'active_hwnd', 'focus_hwnd', 'capture_hwnd'))
            if state != previous:
                lines.append('| ' + f"{snapshot['t_ms']/1000:.3f}" + ' | ' + ' | '.join(map(str, state)) + ' |')
                previous = state
    lines += ['', '### Foreground intervals', '', '| Start (s) | End (s) | State | Foreground PID |', '|---:|---:|---|---:|']
    for index, event in enumerate(focus):
        end = focus[index + 1]['t_ms'] if index + 1 < len(focus) else data['elapsed_ms']
        lines.append(f"| {event['t_ms']/1000:.3f} | {end/1000:.3f} | {event['state']} | {event['foreground_pid']} |")
    lines.append('')
lines += ['## Evidence boundary and next step', '',
          'CONFIRMED: both PIDs have foreground and background samples; windows were restored;',
          'GUI focus/active handles and nonzero mouse capture were observed during the recording.',
          'A nonzero capture HWND confirms sampled Win32 capture state, not the mouse data backend.',
          'Win32 keyboard/message polling, Raw Input registration/read, and DirectInput device polling remain UNKNOWN.',
          'Background input acceptance remains UNKNOWN: this JSON has no key/button log or response oracle.', '',
          'Next: obtain operator scenario details (typing, mouse movement, click/drag, response of each client).',
          'Correlate those details with these measured intervals. Do not infer backend identity from behavior alone.',
          'Backend confirmation requires attributable official diagnostics/source/vendor-supported trace;',
          'do not replace missing evidence with foreign hooks, DLL injection or synthetic EVE input.', '',
          'This report does not establish independent HVI control or an authorized integration surface.', '']
output.write_text('\n'.join(lines), encoding='utf-8')
print('Saved', output)