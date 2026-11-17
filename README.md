# Hunter Virtual Input (HVI)

New experimental backends: typed DirectInput mouse A/W, native Raw Input receive wrapper (no synthetic native handles), and explicitly loaded `hvi_runtime.dll`. See `analysis/reports/DirectInput_Mouse.md`, `RawInput_Backend.md`, `HVI_Runtime.md`, and `Compatibility_Matrix.md`. Transparent support for HVI-unaware applications and Runtime DirectInput keyboard are NOT implemented. CTest labels `unit` and `windows_integration` separate the automated scopes.

Experimental standalone Windows x64 C++17 input model and controlled multi-process test environment. No proprietary binaries, decompiled implementations or downloaded toolchains are included.

## Build

Requires Windows, CMake >=3.20, and a C++17 compiler with Windows SDK/DirectInput headers and libraries (tested with MinGW-w64 GCC 16.2).

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\test_controller.exe 2
```

Ensure compiler runtime DLLs are on PATH. Test scripts locate executables in `build`; adapt that path when using a multi-configuration generator.

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

IPC is synchronous and can block on a stalled peer. Default local pipe security is not production authentication. DirectInput substitution supports a fixed keyboard format, not full device emulation. Mouse state/buffers are logical, not a DirectInput mouse COM backend. See `analysis/reports/HVI_Core_Progress.md` for tested features and remaining gaps; local paths in that historical report describe the original research workspace.