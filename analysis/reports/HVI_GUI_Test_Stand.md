# HVI GUI test stand

## Launch

Build with CMake, then run `build/hvi_demo.exe`. Three independent Win32 processes open: TEST A (session 1), TEST B (session 2), CONTROL. Launcher exits; windows remain independent. Work-area-relative placement leaves clients above the controller. Clients show without activation and never repeatedly take foreground. Native per-monitor DPI awareness is requested; client viewers have vertical scrolling. Controller controls/font scale to client area and DPI changes preserve work-area positioning. Extremely small desktops can reduce legibility; multi-monitor mixed-DPI usability has not been manually verified.

MinGW dependency DLLs are copied alongside binaries by CMake. No toolchain PATH is needed to run the built demo. Do not commit downloaded toolchains or build outputs.

## Architecture and sources

State and devices are in hvi_runtime.dll, not GUI EXEs. Each client process loads its own Runtime and owns one session. GUI polls HviGetKeyboardState, IDirectInputDevice8W keyboard/mouse immediate and buffered APIs every 33 ms. Runtime snapshot provides absolute coordinates, connection/PID/error/timeout counters; API observations, not sent commands, drive key highlighting. Last 20 lines identify API source and local observation time. Runtime diagnostic history separately retains command transition times. Native GetKeyboardState/GetAsyncKeyState and genuine WM_INPUT are clearly separate. F2 toggles native comparison. Wheel scrolls viewer. Raw registration failure is visible; physical-device delivery is hardware/session dependent. No fake HRAWINPUT is created.

Controller IO is on its own thread; UI enqueues commands. Independent transports maintain sequence/ACK/latency per session. Heartbeat every approximately 150 ms normally prevents the 500 ms Runtime idle reset. Failed peers do not block the UI; failed commands are not retried silently. Auto-reconnect starts a fresh sequence and state. NONE sends no user command (connection heartbeats continue). Release All deliberately resets BOTH for safety, regardless of selected target.

## Manual procedure

1. Select TEST A, enable Hold/toggle, click W. Only A highlights W in Win32 and DIK views.
2. Select TEST B, hold S or D. A continues holding W while controller stays foreground.
3. Move inside mouse pad under A, then B; inspect independent coordinates/deltas. LMB/RMB/MMB buttons toggle; pad supports actual down/up. Wheel buttons send +/-120.
4. BOTH + Space delivers to both; NONE + Enter changes neither.
5. KeyDown/KeyUp use the key selector. Physical-entry field routes key messages while focused; leaving it releases its tracked keys. Windows physical input is never globally blocked.
6. Release All clears both devices. Reset Mouse clears only selected mouse state. Disconnect clears selected sessions, followed by heartbeat reconnect.
7. Close A; verify B still receives. Close controller to release/disconnect; close B.

## Automated coverage

CTest `gui_integration` (label gui_integration, serial, interactive desktop required) starts three real processes, manipulates controller controls/mouse pad, queries a versioned WM_COPYDATA observation from each viewer, and checks actual Win32/DirectInput poll results. Covers A/B keyboard and mouse isolation, concurrent holds, BOTH/NONE, heartbeat holds, Release All, entry field routing, disconnect/reconnect, buffered API counts and B survival after A closes. It temporarily moves the physical cursor for the pad test and restores it. Test cleanup waits for processes and fails/terminates only its own children if hung. Close any manually running demo first. This is automated interactive GUI validation, not a claim of exhaustive manual usability or visual review.

No arbitrary third-party compatibility, transparent interception, OS-wide virtual hardware, or Hunter modifications.