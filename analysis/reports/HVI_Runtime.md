# Runtime stage

CONFIRMED scoped: hvi_runtime.dll exports explicit Initialize/Shutdown, virtual keyboard state/async queries, typed A/W keyboard and mouse creation, real/virtual keyboard selector and versioned diagnostics/event history. Per-process state, owned shared feed, mutexes and atomic lifetime flags. DLL starts no threads in DllMain. Caller MUST release every COM object and call Shutdown before FreeLibrary. DLL unloading with live objects is unsupported.

IPC uses separate HVI.runtime.<session> byte pipe, overlapped connect/read/write, stop-event cancellation, 500ms idle/IO timeout, clears state on disconnect and accepts reconnect. Pipe is created synchronously before initialization succeeds. Controller sends heartbeats. User-only DACL and same-user/logon/session impersonated token validation apply before commands. PID, timeout/errors and outstanding objects/IO are exposed. See HVI_Stability_Fixes.md for cancellation retention and trust-boundary limitations. No hard real-time bound is claimed.

UNSUPPORTED: transparent interception in an HVI-unaware application and native Raw Input virtualization. Runtime exports are not standard Windows APIs; runtime_tests and GUI clients explicitly integrate the DLL, not unmodified application compatibility proofs. api_probe remains historical self-instrumentation. Existing keyboard/mouse COM objects remain virtual until released; selector is not a global transparent COM switch.

Existing NamedPipeTransport client now uses timed overlapped IO and invalidates its handle after failed delivery; automatic client reconnect is not implemented. Legacy api_probe/receiver servers still use synchronous IO. Thus repository-wide reliability task remains PARTIAL.

CONFIRMED: runtime lifecycle and two-process tests passed, including state/mouse independent reads, buffered reads, disconnect cleanup, reconnect, COM release and DLL shutdown/unload. These clients explicitly load and call HVI exports; they are not HVI-unaware applications.