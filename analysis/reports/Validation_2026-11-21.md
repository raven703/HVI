# HVI validation — 2026-11-21

## Environment and baseline

- Repository E:\python\HVI, main; clean initial tree, fetched origin/main,
  initial HEAD 2fa34b9. No changes to Hunter/EVE or architecture.
- Windows x64, PowerShell 5.1.22621.6133; OS inventory Windows 10 Pro / 2009
  (reported product strings, not an independently verified marketing release).
- Python 3.13.3, pip 25.3, Git 2.39.1.windows.1; tests use standard library only.
- Repository-local CMake 4.4.5 and WinLibs x86_64 UCRT/SEH GCC 16.2.0 r2,
  MinGW-w64 14.0.0. Tools are ignored, not vendored. Toolchain ZIP SHA256
  D5DBAFC4A170E762CA6143151EC918FB9E2C72736FB14CD704ABEBC6BDD5276A,
  checked against its release checksum before extraction.
- SDL 2.30.11 fetched using the existing pinned SHA256; no SDL source edits.
- Configuration: MinGW Makefiles, RelWithDebInfo, SDL experiment enabled.

## Fixes

1. Runtime DLL lookup: GCC returned a bare DLL filename. CMake now falls back to
   the compiler directory while retaining fatal failure for missing DLLs.
2. Bundled MinGW CMake 4.4.3 had no usable TLS trust anchors; used repository-local
   CMake 4.4.5 with certificate verification enabled. No insecure TLS workaround.
3. Set CMP0135 NEW conditionally for fetched source timestamps, preserving CMake
   3.20 support. Upstream SDL emits a deprecation warning; source is unchanged.
4. GUI mouse assertions intermittently failed from occlusion/external cursor
   movement. The owned controller is raised, hit-tested, and cursor movement is
   stabilized briefly with ClipCursor; previous clip/position restored by RAII.
   Actual pad mouse handler and application API state remain the test oracle.
   This does not test arbitrary foreground-policy enforcement. No game input.
5. Inspector now supports candidate discovery, Unicode CLI, UTF-8 JSON, fixed
   multi-client observation, module/focus/error records, strict failure codes.

## Real test results

- Initial full CTest: 13/13 passed.
- During development, GUI mouse failures were reproduced; not suppressed or
  relabelled as passes. A transient physical-cursor SDL assertion also failed
  during desktop activity. Interactive tests require an undisturbed desktop.
- After the GUI fix: gui_integration passed three consecutive repetitions.
- Final full CTest: **15/15 passed**, total 21.93 s. Evidence:
  E:\python\HVI\build\ctest-final.log and ctest-final.xml.
- New backend_inspector_cli passed: argument/failure cases, JSON/Unicode,
  snapshot repetition, deduplication and owned name-matched process discovery.
- New sdl_demo_launch passed: actual launcher, three visible windows, three PIDs,
  WM_CLOSE cleanup. The test handles the application's dynamic diagnostic titles.
- SDL compatibility passed all 14 object-level checks, including two background
  processes, simultaneous independent keyboard movement, crosshair isolation,
  LMB color only A, wheel scale only B, BOTH/NONE, disconnect/reconnect, native
  restoration and B surviving A close. Evidence:
  E:\python\HVI\build\sdl_compatibility_result.json.
- second_sdl_app and event_delivery_reliability also passed. No claim of EVE
  compatibility follows from owned SDL results.

## Passive EVE evidence

Bounded 5s session sampled exefile.exe PID 4888: five complete snapshots,
299 modules, observed background, no snapshot errors. USER32.dll/win32u.dll
presence confirmed, API execution UNKNOWN. See EVE_Input_Investigation.md.
Raw observation includes local paths and is deliberately not published.

## Reproduce

```powershell
$env:PATH = 'E:\python\HVI\tools\python_packages\cmake\data\bin;E:\python\HVI\tools\installed\mingw64\bin;' + $env:PATH
cmake -S 'E:\python\HVI' -B 'E:\python\HVI\build' -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build 'E:\python\HVI\build' -j 4
ctest --test-dir 'E:\python\HVI\build' --output-on-failure
& 'E:\python\HVI\build\hvi_sdl_demo.exe'
& 'E:\python\HVI\build\input_backend_inspector.exe' --list-eve
& 'E:\python\HVI\build\input_backend_inspector.exe' --eve 10000 --json 'E:\python\HVI\build\eve-observation.json'
```

Close demos before CTest. Do not interact with desktop while GUI tests run.
Inspector does not change focus/input; manual focus switching is optional.
Win32/DI/Raw backend execution stays UNKNOWN without attributable evidence.

## Authoritative API basis

Microsoft Learn references checked for CreateToolhelp32Snapshot (read-only
snapshots, loader races, ERROR_BAD_LENGTH), GetForegroundWindow (may return NULL)
and GetRegisteredRawInputDevices (current application only). The inspector does
not infer foreign registration from its own application's query. Foreground
sampling is not thread-focus tracing, and module absence is not proof of non-use.