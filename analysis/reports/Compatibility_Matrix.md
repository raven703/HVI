# Compatibility matrix

| Feature | Status | Evidence / limitation |
|---|---|---|
| Typed mouse Device8A/W | CONFIRMED scoped x64 | mouse unit test, canonical formats only |
| Immediate/buffered X/Y/Z, five buttons | CONFIRMED scoped | unit + runtime DLL tests |
| Mouse acquire/unacquire, reset | CONFIRMED scoped | not native focus acquisition emulation |
| Two processes, different mouse/key state | CONFIRMED | runtime_two_processes, distinct deltas/button/wheel/key checks per process; no GUI focus dependency |
| DLL initialize/shutdown/COM release/unload | CONFIRMED explicit lifecycle | live objects at FreeLibrary unsupported |
| Runtime IPC reconnect/reset | CONFIRMED | runtime_lifecycle disconnect, reconnect and reset assertions |
| Runtime timed IO | PARTIAL | bounded waits/cancel; no hard real-time guarantee, no authentication |
| Legacy NamedPipeTransport | PARTIAL | client IO now timed, servers still synchronous |
| Raw Input native registration | CONFIRMED | register/query/remove tested |
| Genuine WM_INPUT delivery/buffer/device info | UNKNOWN | wrappers exist, no physical-device automated assertion |
| Synthetic native HRAWINPUT generation | UNSUPPORTED | synthetic records explicitly separate |
| Win32 virtual keyboard DLL exports | CONFIRMED scoped | integrated runtime client, not transparent API hooks |
| DirectInput keyboard Runtime factory | UNSUPPORTED | historical api_probe still self-instruments |
| HVI-unaware Windows API applications | UNSUPPORTED | no controlled loader/interception implementation yet |
| Real/virtual selector | PARTIAL | keyboard exports; existing mouse objects stay virtual |
| x86 | UNKNOWN | only x64 compiler/test run |
| EVE / arbitrary third-party compatibility | UNKNOWN | neither modified nor tested |

This delivery is a partial implementation, not completion of all requested stages. Existing probe focus tests are separate historical evidence, not proof of Runtime focus policy or unmodified application compatibility.