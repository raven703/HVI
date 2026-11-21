# EVE Online: two-client passive input investigation

Date: 2026-11-21. Base repository commit: 18a506a; inspector diagnostic revision
2 built locally with this report. No commit/push requested for this stage.

## Scope and safety

Investigate available passive evidence before choosing an HVI backend. No EVE or
Hunter file changes, no DLL loading into either client, no debugger, injection,
hooks, remote threads, memory reads/writes, privilege elevation, input synthesis,
window activation, window restoration or protection bypass. Only the inspector
and its owned test window were changed/created. HVI runtime architecture and
compatibility layer are unchanged.

The inspector uses Toolhelp module/process snapshots and external window/GUI
queries. It does not subscribe to EVE's message queues or trace API execution.
Module names, window class and image path are observations, not authenticated
publisher/product identification. Character names/window titles are not stored.

## Measurements

1. Baseline: 07:47:22 UTC, 10000ms, existing inspector. Both client candidates
   enumerated successfully, 10 complete snapshots each, 299/298 modules. Both
   background, no observed foreground transition.
2. Extended run: 07:49:25 UTC, requested 30000ms, measured elapsed **30063ms**.
   Inspector `--eve`, observes both startup client candidates simultaneously.

| Measurement | Client A | Client B |
|---|---|---|
| PID | 4888 | 21584 |
| Image basename | exefile.exe | exefile.exe |
| Main HWND (decimal) | 200488 | 3477462 |
| Main GUI thread ID | 21872 | 12364 |
| Main window class | trinityWindow | trinityWindow |
| Main visible / enabled | true / true | true / true |
| Main minimized | true | true |
| Complete module snapshots | 29 | 29 |
| Unique module paths | 299 | 298 |
| Foreground / background / unknown samples | 0 / 483 / 0 | 0 / 483 / 0 |
| Window sample attempts | 114 | 114 |
| Distinct window snapshots | 1 | 1 |
| Module enumeration errors | none | none |
| GUI query errors in stored snapshot | none | none |
| Active / focus / capture HWNDs | 0 / 0 / 0 | 0 / 0 / 0 |

Both first image module paths: `C:\CCP\EVE Online\tq\bin64\exefile.exe`.
Signature/version/hash of EVE itself was not inspected. Each PID also owned two
hidden top-level IME-related windows (MSCTFIME UI and IME) on the same GUI thread.
The main window rectangle was [-32000,-32000,-31840,-31972]. Visibility true does
not mean the minimized window was displayed as a normal restored client.

Foreground owner was PID 13696, HWND 69232 throughout recorded focus observations.
No inference is made about that process's identity. GUI/thread values are samples,
not a guarantee that no short transition occurred. 483 polls over 30s is below a
strict 20Hz rate: the intervals are nominal, snapshots and scheduling add delay.

### Loaded-module comparison

Both clients: USER32.dll, win32u.dll, textinputframework.dll observed.
Neither client: dinput8.dll, dinput.dll, SDL2.dll observed in these snapshots.
Case-insensitive module-name difference: only A had twinapi.appcore.dll; B had
no unique module name relative to A. No input significance assigned to this
difference. Full paths remain in local raw evidence, not in the shared summary.

## Technical conclusion: no input backend confirmed

| Question | Status | Evidence boundary |
|---|---|---|
| Both clients have native Win32 top-level windows | CONFIRMED | Distinct PIDs/HWNDs/TIDs and trinityWindow class |
| Keyboard via WM_KEY*/WM_CHAR or Win32 state polling | UNKNOWN | No attributable message/call observation |
| Mouse via WM_MOUSE* or Win32 state polling | UNKNOWN | No attributable message/call observation |
| Raw Input keyboard registration/read | UNKNOWN | Foreign registration not queried; no execution evidence |
| Raw Input mouse registration/read | UNKNOWN | Same boundary; win32u presence does not identify Raw Input |
| DirectInput keyboard device/polling | UNKNOWN | No device/acquisition/polling evidence |
| DirectInput mouse device/polling | UNKNOWN | Same; absence of dinput DLL in snapshots is not proof |
| Background delivery with restored clients | UNKNOWN | Both were minimized; no physical input/scenario oracle |
| Foreground delivery / focus-loss policy | UNKNOWN | No foreground samples or input acceptance measurements |
| Independent HVI control of EVE | NOT DEMONSTRATED | Owned SDL tests do not establish EVE support |

**No mechanism is selected for integration.** Native Win32 windows and user32
modules confirm GUI infrastructure, not the keyboard/mouse consumption path.
Keyboard and mouse may have different paths; they must be measured separately.
The current experiment cannot distinguish message input, polling, Raw Input and
DirectInput. It also does not measure game-state/UI response or background
acceptance. A minimized-only inventory cannot answer the user's full
foreground/background behavior question.

## Next experiment (manual foreground transitions, passive inspector)

Use normal authorized clients in a safe non-gameplay UI scenario. The operator,
not HVI, restores/activates windows and performs ordinary physical input. Do not
automate EVE input or send it test messages. Suggested 120s observation:

1. Start inspector; record UTC and a scenario log separately. Restore both
   windows. Leave inspector/another neutral application foreground briefly.
2. Manually activate A; type into an authorized safe text field and separately
   move/click within a harmless UI area. Record visible response and whether B
   reacts. Do not use shortcuts/actions affecting gameplay.
3. Manually activate B; repeat the same separate keyboard/mouse observations.
4. Return foreground to a neutral app with both clients restored; record visible
   behavior only. Optionally minimize A then B manually as separate phases.
5. Correlate phase times with focus/window/module snapshots. Confirm that every
   intended foreground/background/minimized phase actually appears in evidence.
   Repeat to distinguish application behavior from one-off scheduling effects.

This can establish **behavior**, not necessarily backend identity. No-response
may be UI policy, focus gating, minimization or acquisition state, not a unique
API signature. Foreground changes alone do not imply background acceptance.

If identity remains UNKNOWN, the next discriminating evidence must be an
existing official application diagnostic/source/vendor-supported trace that
attributes actual input API use to each client PID and scenario. Required fields:
Win32 message/state API names, Raw Input registration flags/device usages and
read calls, or DirectInput factory/device/acquisition/cooperative flags and
GetDeviceState/GetDeviceData, separately for keyboard and mouse. Request such
evidence rather than adding foreign hooks or modifying target binaries. Do not
assume an available ETW provider exposes these calls; no such trace was measured.

Only after attributable evidence: reproduce that exact backend and focus policy
in an **owned** two-process diagnostic harness and add HVI isolation tests there.
Any proposed EVE integration must additionally have an authorized supported
integration surface compatible with the no-injection/no-modification constraint.
Identifying an API alone is not permission or a means to control arbitrary
background processes. If no permitted integration surface exists, stop rather
than claim the current HVI SDL opt-in loader works for EVE.

## Reproduction and evidence

```powershell
& 'E:\python\HVI\build\input_backend_inspector.exe' --list-eve
& 'E:\python\HVI\build\input_backend_inspector.exe' --eve 30000 --json 'E:\python\HVI\build\eve-two-windows.json'
# Operator-assisted phases, no automated focus/input:
& 'E:\python\HVI\build\input_backend_inspector.exe' --eve 120000 --json 'E:\python\HVI\build\eve-manual-phases.json'
```

Raw measurements: E:\python\HVI\build\eve-two-baseline.json,
E:\python\HVI\build\eve-two-windows.json and eve-two-windows.log.
Extended JSON SHA256:
ab99a1190764ba5b46d4210545bb7b933c3a25e37323393ae88a52ed7e964f6e.
Shared redacted summary: EVE_Two_Client_Passive_2026-11-21.json alongside this
report. It preserves sampled windows/counts/status, removes module paths and
candidate launcher inventory. No character/window titles collected by inspector.

Validation: inspector rebuilt; backend_inspector_smoke and backend_inspector_cli
**2/2 passed**. Extended regression verifies an owned hidden Static window and
title privacy. CTest excluding gui_integration/sdl_compatibility labels **10/10
passed**, 6.35s; no new full-15/15 claim. GUI tests were intentionally excluded
to avoid global physical input/focus changes during the EVE study. objdump on
the inspector confirmed external window-query imports, no HVI runtime import;
no SendInput/SendMessage/SetForegroundWindow/OpenProcess/ReadProcessMemory/
WriteProcessMemory/CreateRemoteThread imports in the inspected binary.

## Primary API reference basis (Microsoft Learn, checked 2026-11-21)

- GetGUIThreadInfo (winuser.h): state for a specified GUI thread; return status
  must distinguish failure from successful null handles. Does not report input
  backend or API execution.
- GetRegisteredRawInputDevices (winuser.h): **current application** registrations,
  not arbitrary foreign PID. Calling it in inspector would not describe EVE.
- Raw Input Overview: WM_INPUT/GetRawInputData or GetRawInputBuffer;
  RIDEV_INPUTSINK can allow background reception. This is API capability, not
  evidence that EVE enabled it.
- Cooperative Levels / Acquiring Devices (DirectInput): acquisition and focus
  behavior depend on cooperative flags. No EVE flags were observed here.