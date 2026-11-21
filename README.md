# Hunter Virtual Input (HVI)

Standalone Runtime now includes typed DirectInput keyboard and mouse A/W, virtual Win32 keyboard exports, authenticated-local-session IPC and a three-window Win32 GUI stand. Native Raw Input is receive-only: no synthetic native handles. Transparent support for HVI-unaware applications is NOT implemented.

## Validation update: 2026-11-21

Windows x64, Python 3.13.3, MinGW-w64 GCC 16.2.0 (UCRT/SEH), CMake
4.4.5: build succeeded and full CTest **15/15 passed**. Actual SDL launcher
opens three visible windows with distinct PIDs. Keyboard/mouse isolation is
checked against application-produced results, including both SDL targets in
background. See `analysis/reports/Validation_2026-11-21.md` for evidence and limits.

## Passive input backend inspector

```powershell
& 'E:\python\HVI\build\input_backend_inspector.exe' --list-eve --json 'E:\python\HVI\build\eve-candidates.json'
& 'E:\python\HVI\build\input_backend_inspector.exe' --eve 10000 --json 'E:\python\HVI\build\eve-observation.json'
& 'E:\python\HVI\build\input_backend_inspector.exe' self 200 --json 'E:\python\HVI\build\self.json'
```

An explicit PID can replace `self`. `--eve` observes name-matched client
candidates only; launchers/crash monitors are listed separately. Discovery is
not verified product identity. Observation uses read-only Toolhelp snapshots
and sampled foreground PID/HWND, never target input, hooks, process memory,
injection or privilege escalation. Win32/DirectInput/Raw Input **execution
remains UNKNOWN**, regardless of DLL presence. JSON includes timestamps,
modules, focus timeline, errors and limitations; local paths may be sensitive.
See `analysis/reports/Input_Backend_Inspector.md` and `EVE_Input_Investigation.md`.

## SDL2 compatibility experiment (separate from cooperative GUI)

```powershell
.\build\hvi_sdl_demo.exe
```

Close the original HVI demo first (same sessions 1/2). Two independent SDL-only
test EXEs open with existing Control Panel. Select A/B, enable Hold/toggle and
send W/S/D; squares move in background. Mouse pad moves crosshair; LMB changes
color, RMB reverses movement, wheel changes scale. Foreground SDL + F12 switches
that process to native input; R toggles relative mouse mode. Native baseline:
`.\build\sdl_input_test.exe`.

SDL 2.30.11 source is fetched from official archive with pinned SHA256 during
configure (network needed on first build). Disable with `-DHVI_SDL_EXPERIMENT=OFF`.
Compatibility uses SDL's opt-in `SDL_DYNAMIC_API` loader, not remote injection.
The SDL test EXE has no HVI API, headers, sessions or command pipe. This is a
selected-SDL-API experiment, NOT generic Windows/DirectInput/Raw Input compatibility.
Read the four compatibility/focus reports in `analysis/reports` before reuse.

```powershell
ctest --test-dir build -R sdl_compatibility --output-on-failure
```

Requires an interactive unlocked desktop, briefly sends native keys/moves cursor.
Checks actual object results from a read-only JSON output, not IPC ACKs.

## Original cooperative GUI

After building, run from the repository root:

```powershell
.\build\hvi_demo.exe
```

TEST A and TEST B are separate processes/sessions. In CONTROL select TEST A, enable Hold/toggle and click W; select TEST B and click S. Only the selected client's virtual Win32/DirectInput indicators change, even while CONTROL remains foreground. Move in the mouse pad to send relative motion to the selected target. BOTH sends separately to both; NONE sends no user input. Release All resets BOTH for safety. Physical-entry field, explicit KeyDown/KeyUp, left/right modifiers, mouse buttons/wheel, Reset Mouse and Disconnect are available. F2 in a viewer toggles clearly labelled native comparison. Scroll viewers to see event history.

See `analysis/reports/HVI_GUI_Test_Stand.md`, `DirectInput_Keyboard.md`, and `HVI_Stability_Fixes.md` for architecture, manual checks and limitations.

Experimental standalone Windows x64 C++17 input model and controlled multi-process test environment. No proprietary binaries, decompiled implementations or downloaded toolchains are included.

## Build

Requires Windows, CMake >=3.20, and a C++17 compiler with Windows SDK/DirectInput headers and libraries (tested with MinGW-w64 GCC 16.2).

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\test_controller.exe 2
```

For MinGW builds CMake copies required runtime DLLs alongside the demo. Test scripts locate executables in `build`; adapt that path when using a multi-configuration generator. GUI integration requires an interactive unlocked Windows desktop and no other HVI demo running; it briefly moves the cursor.

For this workstation's repository-local tools (not committed), use:

```powershell
$env:PATH = 'E:\python\HVI\tools\python_packages\cmake\data\bin;E:\python\HVI\tools\installed\mingw64\bin;' + $env:PATH
cmake -S 'E:\python\HVI' -B 'E:\python\HVI\build' -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build 'E:\python\HVI\build' -j 4
ctest --test-dir 'E:\python\HVI\build' --output-on-failure
& 'E:\python\HVI\build\hvi_sdl_demo.exe'
```

Put this CMake **before** MinGW's bundled CMake in PATH: the bundled build
lacked a usable TLS trust store on this machine. Certificate verification was
not disabled. GUI tests must run without concurrent desktop interaction;
`gui_integration` briefly clips the cursor and restores its previous clip and
position using an RAII guard. Only owned test windows are raised/closed.

## Controller

Console controller launches 2–16 owned probe processes. Commands, one per line:

```text
target 1
down 65
state
up 65
target 2
button 0 1
move 10 -5
absolute 200 300
wheel 120 0
reset
quit
```

## Tests

```powershell
python tests/api_isolation.py
python tests/extended_input.py
python tests/focus_input.py
python tests/integration.py
```

Desktop tests require an interactive Windows session. The focus test explicitly changes foreground among owned test windows; Windows policy may make it inconclusive. Logs go to `analysis/reports`.

## Scope and limitations

Keyboard/mouse state, ordered bounded transition queues, a cooperative adapter interface and named-pipe routing are implemented. The probe substitutes only its own Win32 imports and methods on one owned DirectInput keyboard object. This is not OS-wide virtual hardware, arbitrary application support or game injection.

New Runtime transport has bounded overlapped waits, cancellation, per-session sequence checks and same-user/logon/session identity checks. Legacy probe servers remain synchronous. Runtime supports canonical DirectInput keyboard and mouse formats, not full native device emulation; foreground/exclusive and arbitrary formats are unsupported. DLL callers must release COM devices and shut down before unloading. See the new reports for current scope; `HVI_Core_Progress.md` is historical.