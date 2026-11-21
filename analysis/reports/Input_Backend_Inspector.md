# Input Backend Inspector

## Two-client diagnostic revision 2: 2026-11-21

Existing schema version 1 gains additive `diagnostic_revision: 2` fields:
`focus_samples` counts foreground/background/unknown polls per target;
`window_samples` counts attempted window snapshots; `window_changes` stores the
first and changed top-level snapshots. No CLI change. EnumWindows, GetClassNameW,
GetWindowThreadProcessId, GetWindowRect, IsWindowVisible/IsIconic/IsWindowEnabled
and GetGUIThreadInfo are read-only observations; no target messages are sent.
Window titles are deliberately not read or stored. Each window includes HWND,
TID, class, visibility/minimized/enabled state, rectangle and GUI-thread
active/focus/capture HWNDs, flags and query errors. Failed GUI queries have
UNKNOWN status and null handles, distinct from a successful zero handle.

Window cadence is nominally 250ms; module snapshots and scheduling can delay it.
Counters are samples, not exact duration or a delivery oracle. EnumWindows covers
top-level windows on the inspector's desktop, not child/message-only windows or
all threads. Per-thread focus/capture is not proof of input API calls, foreground
ownership or background input acceptance. Snapshots are non-atomic and can race
window destruction. Existing process/PID-lifetime limitations still apply.

Tests: backend_inspector_smoke and backend_inspector_cli pass. CLI regression now
creates an owned hidden Static window, verifies HWND/TID/class/GUI state and
checks that a private window title is absent from JSON. The inspector test does
not activate or send input to EVE. See EVE_Two_Client_Passive_2026-11-21.md.

## 2026-11-21 update (supersedes CLI/validation below)

`input_backend_inspector.exe <pid|self|--eve|--list-eve> [duration-ms] [--json path]`
supports Unicode command-line paths and UTF-8 JSON schema version 1. The original
PID/self syntax remains supported. `--help` documents usage. Duration is decimal
100..600000 ms (default 5000); signed, trailing-junk and duplicate arguments fail.

`--list-eve` inventories exact case-insensitive names exefile.exe/eve.exe (client
candidates), eve-online.exe (launcher candidates), eve_crashmon.exe (monitor
candidates). Identity is always UNKNOWN; this is not signature/product validation.
`--eve` samples every discovered client candidate, excluding launcher/monitor
processes. The target list is fixed at startup; restart/PID reuse is not resolved.
Explicit PID works for any permitted process. No EVE launch or foreground change
is performed. Inventory is observational, not a command channel.

JSON contains UTC start, elapsed/requested time, candidates, PID/name, first-seen
module names/paths, number of complete module snapshots, foreground PID/HWND/state
changes, Win32 errors and mechanism statuses. HWND is a decimal string to avoid
JSON number precision loss. Null foreground is UNKNOWN, not background. Module
snapshots occur approximately every second; foreground polling every 50ms (not
hard real-time). An observation is CONFIRMED only within these sampling limits.
No unload tracking; no cross-process Raw Input registration query. All execution
statuses stay UNKNOWN. Output is written at normal completion; interruption can
leave an empty/incomplete file. Existing destination is truncated. Redact paths
and inventory before sharing; raw live-process data is retained under build only.

Exit codes: 0 = list succeeded, or at least one complete module enumeration for
**each** selected target; 1 = missing client candidates/incomplete observation;
2 = invalid CLI; 3 = output/internal error. Neither 0 nor module presence proves
input API execution. Errors remain in the JSON even after a successful retry.

Safety: only Toolhelp snapshot handles and desktop foreground reads; no
OpenProcess, target memory read/write, debugger, input hooks, injection, remote
threads, HVI runtime initialization or elevation. ERROR_BAD_LENGTH retries are
bounded to five attempts; snapshot handles use RAII.

Validation: backend_inspector_smoke and backend_inspector_cli passed in full
15/15 CTest. CLI test covers strict arguments, Unicode executable/output paths,
UTF-8 JSON parsing, repeated snapshots, module deduplication, focus state,
terminated owned PID, invalid output directory and deterministic name discovery
using an owned inspector copy called exefile.exe. It never launches EVE. Live
candidate observation is documented separately in EVE_Input_Investigation.md.

Date: 2026-11-17. Standalone Windows x64 console executable `input_backend_inspector`. No HVI runtime initialization or compatibility DLL loading. Usage from build directory:

```powershell
.\input_backend_inspector.exe self 200
.\input_backend_inspector.exe 1234 10000 > input-observation.log
```

PID is explicit; duration 100..600000 milliseconds (default 5000). Exit codes: 0 at least one module enumeration succeeded; 1 no enumeration succeeded; 2 invalid arguments. This is not an API-tracing success code.

CONFIRMED observations: module names and paths via CreateToolhelp32Snapshot / Module32FirstW / Module32NextW, sampled once per second; foreground HWND and owning PID via GetForegroundWindow / GetWindowThreadProcessId, sampled every 50ms. New modules and foreground changes logged with elapsed times. Snapshot failures include Win32 error, with bounded ERROR_BAD_LENGTH retry. No debugger, process memory access, DLL injection, remote thread, hooks, elevation or target modification.

LIKELY: presence of dinput8.dll means DirectInput8 is available in that process. It does not establish device creation, keyboard/mouse selection, acquisition or polling. All actual DirectInput8 calls remain UNKNOWN. user32/win32u presence similarly does not establish keyboard polling.

UNKNOWN: actual Raw Input registration/read calls and Win32 keyboard/mouse calls. GetRegisteredRawInputDevices documents current-application scope, so querying the inspector's registrations would not describe a foreign target. No misleading cross-process query is made. Module absence is not proof of non-use, snapshots can race load/unload; protected processes can deny inspection. Sampling misses short focus transitions and is not thread-level focus tracing. Modules are logged when first seen; unloads are not tracked. Paths may contain local account names: redact before sharing.

Validation: self smoke CTest and passive enumeration of both owned official SDL processes. Existing build/sdl_compatibility_result.json contains selected SDL Win32 call observations from the separately instrumented owned harness; not evidence about another PID and not imported as proof here.

Authoritative API references: Microsoft Learn, functions CreateToolhelp32Snapshot (tlhelp32.h), GetRegisteredRawInputDevices (winuser.h), GetForegroundWindow and GetWindowThreadProcessId. In particular the Raw Input query explicitly describes the current application, not arbitrary PID.