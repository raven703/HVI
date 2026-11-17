# EVE input investigation

Date: 2026-11-17.

CONFIRMED local observation: process-name inventory (`Get-Process`, names matching eve or exefile) returned no running candidate during this stage. No EVE PID was supplied. This is not proof EVE is uninstalled, nor a comprehensive executable-identity search. No EVE launch, attachment, injection, memory access, process modification or protection bypass performed.

| Mechanism | Status | Evidence / boundary |
|---|---|---|
| EVE loaded input modules | UNKNOWN | No target process snapshot |
| DirectInput 8 keyboard/mouse | UNKNOWN | No EVE API execution evidence |
| Raw Input | UNKNOWN | No EVE registration/read evidence |
| Win32 keyboard/mouse polling/messages | UNKNOWN | No EVE call evidence |
| SDL backend | UNKNOWN | Independent SDL success says nothing about EVE |
| Foreground HWND/PID and focus policy | UNKNOWN | No EVE observation session |

No mechanism is marked LIKELY merely from game category or third-party speculation. The main deliverable is a repeatable passive observation path, not a fabricated backend identification.

Next authorized session: user starts EVE normally, supplies its verified PID; run input_backend_inspector with bounded duration while manually switching foreground/background. Save module paths, observed focus timeline, errors, version and scenario. Do not load hvi_compat into EVE. Correlate only existing user-provided logs or documented application diagnostics. dinput8 presence could support LIKELY availability, never CONFIRMED polling. Raw Input/Win32 execution stays UNKNOWN unless attributable documented evidence exists. If passive evidence cannot distinguish backends, stop with UNKNOWN rather than add invasive hooks. No claim that an appropriate compatibility backend has been selected.