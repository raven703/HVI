# Experimental keyboard PostMessage tool

Mouse extension: E:\python\HVI\analysis\reports\EVE_Mouse_Post_Experiment.md
documents per-HWND virtual coordinates, left/right buttons and wheel commands.
The same executable/log is used; `release` now also releases tracked mouse buttons.

## Operator result update

See E:\python\HVI\analysis\reports\EVE_Key_Post_Result_2026-11-21.md.
The inspected operator log contains successful queued ESC A/B/BOTH trials with
manual yes confirmations, plus W Down/Up A. Do not interpret the earlier
"EVE NOT TESTED" owned-test result as this later operator session's outcome.
Other keys, mouse, background state polling and backend exclusivity remain
unverified; the result report separates sender measurements from user feedback.

Date: 2026-11-21. Standalone target `eve_key_post`, source
E:\python\HVI\experiments\eve_key_post.cpp. Not a new HVI transport and not
evidence of EVE compatibility. No Hunter/EVE edits, hooks, injection, process
memory access, SendInput or focus/activation APIs. Only the requested window
keyboard messages are posted, following explicit console commands.

## Launch

```powershell
& 'E:\python\HVI\build\eve_key_post.exe'
```

Discovery-only: `E:\python\HVI\build\eve_key_post.exe --list`.
Log: E:\python\HVI\build\eve_key_post.log (UTF-8 append, UTC timestamps).
Requires an interactive Windows console. Runtime DLLs already reside in build.

Automatic discovery: visible top-level windows with class `trinityWindow` owned
by name-matched `exefile.exe` processes. This is candidate identity, not signature
verification. Titles/character names are not collected. Windows are sorted by
PID/HWND; first is A, second B. BOTH means those first two, not arbitrary extra
clients. Numeric selection permits another listed window. No default selection.
Check displayed PIDs/HWNDs before sending. At 08:07:44 UTC measured inventory:
A PID 4888 HWND 200488; B PID 21584 HWND 3477462; both not minimized.

Before every post, HWND/PID/TID/class/process-name are revalidated. This narrows
stale handle errors but cannot eliminate reuse/races or authenticate process
creation identity without additional handles. No broadcast/null HWND posting.
Refresh releases tracked keys before replacing the list, and clears selection;
failed release refuses refresh. No automatic restore/activation of EVE windows.

## First manual test: ESC

Restore both clients **manually**, close menus/dialogs to a known safe state and
leave this console foreground. The sender does not activate targets. Do not use
gameplay actions. Execute commands individually, observing after each press:

```text
select A
key ESC
press
confirm yes
```

`confirm yes` means **you observed** the expected menu response in the selected
client. Use `confirm no` if not, or `confirm unknown` if uncertain. Check both
windows: did only A react? Record wrong-recipient effects separately. ESC's
opening/closing effect is a hypothesis under test, not a guaranteed Windows
system-menu operation. Do not enter `confirm yes` merely because QUEUED appears.

Then test B and BOTH separately, resetting menus manually if needed:

```text
select B
key ESC
press
confirm no
select BOTH
key ESC
press
confirm unknown
```

The example confirmations above are placeholders for the operator's actual
yes/no/unknown response, not predicted results. For BOTH, yes should mean both
reacted; if only one reacted, use unknown and describe each client's result.
Confirmation logs current selection/key, not an independently verified event
identifier. Record confirmation immediately without changing selection/key first.

Only after ESC, repeat separately for `key A`, `key W`, `key S` in a safe UI
context. No WM_CHAR is posted explicitly. This is not a guarantee of text entry
or state-polling compatibility. Do not infer backend identity from one response.

## Holding / releasing

```text
select A
key A
down
up
select B
key W
down
release
quit
```

`press`: KEYDOWN, 80ms wait, KEYUP, to each selected recipient. BOTH uses separate
posts (not atomic or hardware-simultaneous). `down` posts one KEYDOWN without
automatic repeat; another down sets previous-state bit. `up` posts KEYUP to
current selected windows. `release` sends KEYUP for **all tracked successfully
posted down keys**, including formerly selected recipients. It does not touch
unrelated physical keys or broadcast every possible VK. `press` on a tracked
held key is refused; use up/release first. Normal quit/EOF performs release.

Release is best effort: a failed post retains tracking; retry or handle the
client manually. Queue acceptance is not application key release. Abrupt close,
Ctrl+C/process termination/crash cannot guarantee cleanup. Prefer `release`, then
`quit`, not closing the console while down is held. Do not run concurrent senders.

## Diagnostics and evidence boundary

Every post logs PID, decimal HWND, VK, message, lParam, QUEUED/FAILED, Win32 error
and foreground HWND before/after. An external foreground change is observable;
the tool itself never requests it. Validation failure uses error 1400. UIPI
error 5 is reported without elevation or bypass. Unsupported command line
returns 2; runtime error/failed normal-exit release returns 1. Per-command post
failure is logged and the interactive session continues.

PostMessage returns before message processing (Microsoft Learn PostMessageW),
so **QUEUED != handled != visible response**. lParam uses repeat=1, target-layout
scan code, KEYUP previous+transition bits, and KEYDOWN previous bit for repeats.
Only ESC/A/W/S non-extended keys are allowed. No foreign keyboard-state arrays,
Raw Input records, DirectInput devices or OS physical keyboard state are changed
by this tool. A negative test does not establish which backend EVE uses.

## Validation performed, not EVE response claims

- Built with existing CMake/MinGW toolchain.
- `eve_key_post_owned` CTest passed: hidden owned windows A/B/BOTH, received
  down/up ordering and isolation, ESC identity, lParam scan/repeat/transition,
  A/W holds, repeated down, explicit A up, Release All receipt, S press and
  destroyed-HWND failure. Foreground handle unchanged in owned test.
- Interactive CLI subprocess without selection: press refused; invalid key and
  selection rejected; no POST records to EVE. Invalid command line returns 2.
- `--list` found the two real clients without posting keyboard messages.
- Import inspection: PostMessageW present; no SetForegroundWindow, SetFocus,
  SendInput, OpenProcess, ReadProcessMemory, WriteProcessMemory, SetWindowsHook
  or CreateRemoteThread imports; no HVI runtime/compat DLL dependency.
- Automated tests used only owned test windows, not EVE. **EVE A/B/BOTH reaction
  remains UNKNOWN and must be confirmed manually by the operator.**

Rebuild/test:

```powershell
$env:PATH = 'E:\python\HVI\tools\python_packages\cmake\data\bin;E:\python\HVI\tools\installed\mingw64\bin;' + $env:PATH
cmake -S 'E:\python\HVI' -B 'E:\python\HVI\build' -G 'MinGW Makefiles'
cmake --build 'E:\python\HVI\build' --target eve_key_post -j 4
ctest --test-dir 'E:\python\HVI\build' -R '^eve_key_post_owned$' --output-on-failure
```