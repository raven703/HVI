# Compatibility matrix

| Feature | Status | Evidence / limitation |
|---|---|---|
| Typed mouse Device8A/W | CONFIRMED scoped x64 | mouse unit test, canonical formats only |
| Immediate/buffered X/Y/Z, five buttons | CONFIRMED scoped | unit + runtime DLL tests |
| Mouse acquire/unacquire, reset | CONFIRMED scoped | not native focus acquisition emulation |
| Two processes, different mouse/key state | CONFIRMED | runtime_two_processes, distinct deltas/button/wheel/key checks per process; no GUI focus dependency |
| DLL initialize/shutdown/COM release/unload | CONFIRMED explicit lifecycle | live objects at FreeLibrary unsupported |
| Runtime IPC reconnect/reset | CONFIRMED | runtime_lifecycle disconnect, reconnect and reset assertions |
| Runtime timed IO / identity | CONFIRMED scoped | bounded waits, stop cancellation, same-user/logon/session token checks; not hard real-time or EXE authentication |
| Legacy NamedPipeTransport | PARTIAL | client IO now timed, servers still synchronous |
| Raw Input native registration | CONFIRMED | register/query/remove tested |
| Genuine WM_INPUT delivery/buffer/device info | UNKNOWN | wrappers exist, no physical-device automated assertion |
| Synthetic native HRAWINPUT generation | UNSUPPORTED | synthetic records explicitly separate |
| Win32 virtual keyboard DLL exports | CONFIRMED scoped | integrated runtime client, not transparent API hooks |
| DirectInput keyboard Runtime factory | CONFIRMED scoped | typed A/W, canonical format, DIK modifiers, unit/runtime/GUI tests |
| Three independent GUI windows | CONFIRMED scoped | launcher, controller, API-driven viewers, GUI integration test |
| HVI-unaware Windows API applications | UNSUPPORTED | no controlled loader/interception implementation yet |
| Real/virtual selector | PARTIAL | keyboard exports; existing mouse objects stay virtual |
| x86 | UNKNOWN | only x64 compiler/test run |
| EVE / arbitrary third-party compatibility | UNKNOWN | neither modified nor tested |

See HVI_GUI_Test_Stand.md, DirectInput_Keyboard.md and HVI_Stability_Fixes.md for current evidence. Existing probe focus tests are separate historical evidence, not proof of unmodified application compatibility. Foreground/exclusive acquisition policy remains unsupported; stand deliberately uses background/nonexclusive devices.