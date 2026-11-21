# EVE input investigation

## Active message experiment: operator session 2026-11-21 08:15 UTC

EVE_Key_Post_Result_2026-11-21.md records ESC A/B/BOTH PostMessage trials with
three manual yes confirmations and W Down/Up A. Successful queueing and visible
ESC response support the tested UI message-delivery path without observed
foreground changes at send boundaries. They do not identify the full physical
keyboard backend or any mouse backend; Raw Input/DirectInput use remains UNKNOWN.

## Manual phases recorded: 2026-11-21 07:54:43 UTC

EVE_Manual_Phases_2026-11-21.md analyzes the 120031ms operator recording.
Both clients now have measured foreground/background samples with restored
windows and nonzero GUI mouse-capture states. This supersedes the minimized-only
behavior coverage below. Input backend and background input acceptance still
remain UNKNOWN: no key/button events or application response oracle were recorded.
Next step is correlating the operator's actual actions/responses with this timeline.

## Latest session: two clients, 2026-11-21 07:49 UTC

See EVE_Two_Client_Passive_2026-11-21.md and the accompanying redacted JSON.
PIDs 4888 and 21584 were observed concurrently; both were minimized/background
throughout the 30s window. Foreground/input acceptance remains unmeasured.
Inspector diagnostic revision 2 adds top-level window/GUI-thread snapshots.
**No keyboard or mouse input backend is confirmed.** Win32, Raw Input and
DirectInput execution all remain UNKNOWN. This session supersedes the prior
single-client inventory as the latest observation, not as execution evidence.

## Passive session: 2026-11-21 (supersedes the no-process observation below)

At 07:26:17 UTC, `--eve 5000 --json build/eve-passive.json` found nine name-based
candidates: seven eve-online.exe launcher processes, one eve_crashmon.exe and
one exefile.exe client candidate, PID 4888. Only the client was module-sampled.
First image path was `C:\CCP\EVE Online\tq\bin64\exefile.exe`; no signature,
publisher or executable-version verification was performed. Discovery therefore
remains candidate identity, not authenticated product identification.

CONFIRMED within the session: five complete module snapshots, 299 unique loaded
module paths, no enumeration errors. USER32.dll, win32u.dll and
textinputframework.dll were present. dinput8.dll and SDL2.dll were not observed
in those snapshots; that does not establish non-use. The single recorded focus
state was background (foreground PID 19200); no foreground transition for the
client was observed. No automated foreground activation, input delivery or EVE
interaction was attempted. Raw paths remain local in build/eve-passive.json and
build/eve-passive.log, not committed.

| Mechanism | Status | Evidence / next permitted step |
|---|---|---|
| Win32 keyboard/mouse calls or messages | UNKNOWN | user32/win32u presence is not execution evidence |
| DirectInput keyboard/mouse | UNKNOWN | No attributable API/device/acquisition evidence |
| Raw Input registration/read | UNKNOWN | No attributable registration/read evidence |
| SDL input backend | UNKNOWN | No SDL execution evidence; owned SDL results are unrelated |
| Background input acceptance / foreground policy | UNKNOWN | Background sampling alone is not an input acceptance test |
| Module inventory and sampled background state | CONFIRMED | Limited to the bounded observation above |

Repeat only with an authorized normally running client. Record inspector version
(Git commit), UTC, verified PID/image, duration and user-described scenario; a
user may manually switch focus during observation. Do not synthesize EVE input.
Correlate only user-provided existing logs or documented application diagnostics.
Without attributable execution evidence, retain UNKNOWN and do not select or
load an HVI compatibility backend. Do not add intrusive tracing to resolve an
UNKNOWN result. Hunter and EVE files/process state were not modified.

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