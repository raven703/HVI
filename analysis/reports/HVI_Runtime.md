# Runtime stage

PARTIAL: hvi_runtime.dll exports explicit Initialize/Shutdown, virtual keyboard state/async queries, typed A/W mouse creation, real/virtual keyboard selector. Per-process globals, owned shared mouse feed, mutexes and atomic lifetime flags. DLL starts no threads in DllMain. Caller MUST release every COM object and call Shutdown before FreeLibrary. DLL unloading with live objects is unsupported.

IPC uses separate HVI.runtime.<session> byte pipe, overlapped connect/read/write, cancellation, 500ms idle/IO timeout, clears state on disconnect, loops to accept reconnect. Idle controller must send heartbeats/snapshot within 500ms. No authentication/hardened ACL. Thread start returns before pipe creation; clients retry. Shutdown normally waits at most an IO timeout plus scheduling; not a hard real-time bound.

UNSUPPORTED: transparent interception in an HVI-unaware application, DirectInput keyboard factory, native Raw Input virtualization. Runtime exports are not standard Windows APIs; runtime_tests is explicitly an integrated DLL test, not an unmodified application compatibility proof. api_probe remains historical self-instrumentation; its extraction into runtime is NOT complete. No hidden fallback claims. Mouse objects created in virtual mode remain virtual until released; selector is not a global transparent COM switch.

Existing legacy NamedPipeTransport/api_probe still use synchronous IO; only new Runtime uses bounded overlapped path. Thus repository-wide reliability task remains PARTIAL.