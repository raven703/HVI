# Input Backend Inspector

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