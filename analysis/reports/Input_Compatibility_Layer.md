# HVI Input Compatibility Layer audit and implementation

2026-11-21. Goal: independent application-visible virtual input per process,
similar in purpose (not implementation or compatibility claims) to a virtual
input environment. PostMessage experiment remains separate and is NOT a backend
or fallback for this architecture. Main Hunter and EVE are unchanged.

## Actual architecture and audit

Hunter Slave (future) -> hvi::InputSession -> existing RuntimeTransport -> local
authenticated named pipe -> process-local Runtime State -> application API
adapters (cooperative Win32 exports, owned DirectInput A/W devices, SDL dynapi).
The non-destructive bounded command journal feeds SDL separately from the
DirectInput transition queues. Runtime ACK means command applied to State,
not API consumption or visible application response.

| Component | Present | Gap / next work |
|---|---|---|
| State | per-process keyboard, 5 buttons, coordinates, relative totals, two wheels | int64 accumulated arithmetic bounds, repeat/text policy |
| IPC | bounded overlapped I/O/cancellation, sequences, same-user/logon/session checks | capability negotiation; old/new peer feature detection; explicit leases |
| Journal | 16384 commands, independent cursor, overflow snapshot recovery | durable/exportable tracing, richer consumer telemetry |
| Win32 | explicit HviGetKeyboardState, async edge, new current HviGetKeyState and HviGetMouseState | not transparent imports; no message-queue historical state/text/IME; no screen cursor facade |
| DirectInput | owned keyboard/mouse Device8 A/W, canonical formats, immediate/buffered reads | no transparent DirectInput8Create factory, enumeration, arbitrary formats, exclusive/foreground emulation, notifications |
| SDL | pinned SDL2 2.30.11 opt-in SDL_DYNAMIC_API, state/events/wait/pump | single tracked window, incomplete key mapping/modifiers, text/IME, high-DPI/multi-window policy, thread-safety and warp/global mouse coverage |
| Raw Input | native receive and synthetic packet codec | NOT synthetic HRAWINPUT/WM_INPUT provider; no device registration/read facade |
| Attachment | cooperative Runtime load or supported SDL loader | no generic foreign-process attachment mechanism |

## Implemented in this change

- E:\python\HVI\include\input_session.hpp is the reusable Slave-facing C++17
  interface: KeyDown/Up(VK), MouseMove(application x/y), MouseMoveRelative(dx/dy),
  MouseDown/Up(button 0..4), Wheel(vertical,horizontal), ReleaseAll, Heartbeat.
  It reuses RuntimeTransport; no second IPC stack/state model/controller.
- InputSession(session, expectedPid) can pin the named-pipe server PID. Session
  is still the route, not HWND. A pin of 0 disables PID checking for existing
  demos. PID alone does not defend against PID reuse: future Slave should own a
  process lifetime handle and an explicit launch/session association.
- Local argument validation avoids poisoning a healthy session with bad controls.
- New appended Kind::ReleaseAll=11 releases all keys/buttons, generates ordered
  transitions and journal entry, preserves coordinates and cumulative deltas /
  wheels and keyboard toggles. Reset/ResetMouse/disconnect retain older reset
  semantics. Existing command numbers and 28-byte Event ABI are unchanged.
  There is no capability negotiation: deploy rebuilt matched Runtime/compat/client;
  old peers reject unknown kind 11. Do not silently fall back to Reset.
- New versioned HviMouseState export: non-destructive int64 application-local
  position/totals, five buttons, generation. Native mode returns unsupported
  rather than moving/reading a fake OS cursor. HviGetKeyState is current virtual
  state, NOT message-queue GetKeyState equivalence.
- Existing GUI worker now uses InputSession and position-preserving ReleaseAll.
  Its mouse button toggles are per-recipient rather than one shared boolean.
- SDL journal adapter translates ReleaseAll into key/button-up events without
  recentering. Existing keyboard/mouse devices share process-local State, never
  the other target's State. No system focus/cursor mutation is part of commands.

## Integration contract for future Slave

```cpp
#include "input_session.hpp"

bool driveSlaveTarget(UINT session, DWORD ownedTargetPid) {
    hvi::InputSession input(session, ownedTargetPid);
    if (!input.Heartbeat()) return false;
    bool ok = input.MouseMove(200, 150);
    ok = input.KeyDown('W') && ok;
    ok = input.MouseDown(0) && ok;
    ok = input.Wheel(120) && ok;
    // Call Heartbeat at <=150ms from the owning IO loop while holding input.
    // Runtime currently drops/reset idle connections after about 500ms.
    bool released = input.ReleaseAll();
    input.disconnect();
    return ok && released;
}
```

One instance per target, thread-confined; use the existing worker queue for
multi-thread callers. Destructor disconnects; it does not block on ReleaseAll.
Explicitly release before teardown; disconnect/idle reset is the safety net.
Failure after a write may be ambiguous: never blindly retry relative movement,
wheel, or clicks. Recover/reconnect to known state and inspect diagnostics.
No hidden heartbeat worker; callers own scheduling. BOTH is fan-out to separate
sessions, not one shared device or transaction. Coordinates are application
local, not automatically DPI scaled, normalized HID, desktop or window messages.
SDL currently clamps to its window; core position is unbounded int32 absolute /
int64 accumulated. Define mapping explicitly per application before integration.

## Verification and evidence

Build logs: E:\python\HVI\build\icl-configure.log and icl-build.log.
CTest logs: E:\python\HVI\build\icl-full-tests.log and icl-targeted.log.

### Measured final validation

- Configure and full build completed with exit 0.
- Core run excluding gui_integration label: 16/16 passed (icl-core-tests.log).
- Separate GUI integration and SDL compatibility retry: 2/2 passed
  (icl-desktop-retest.log); SDL launcher also passed in full runs.
- Final single full run: 18/19 passed (icl-final-tests.log). The sole failure
  was the separate eve_key_post_owned experiment's assertion "foreground changed
  during owned test". All Input Compatibility Layer tests, Win32/DI GUI and SDL
  tests passed in that final run. This is NOT a 19/19 clean full-suite claim.
- Earlier full runs also recorded GUI observation/native foreground refusal
  and a crosshair test failure; later reruns passed. These remain evidence of
  desktop-fixture sensitivity, not hidden successes.
- Python new test modules compile; git diff --check passed.

New input_session_api checks IPC/PID pin, input validation, cooperative keyboard
and mouse state, DirectInput immediate/buffered data, heartbeat holds, explicit
up, position-preserving ReleaseAll, journal command, disconnect/reconnect and
native-mode rejection. input_session_process_isolation runs two distinct Runtime
processes concurrently (sessions 123/124), different keys/buttons/coordinates.
SDL reliability test exercises the SAME InputSession API, verifies state and
ReleaseAll up events with preserved position, plus existing burst/overflow tests.
virtual_input_import_guard checks Runtime and the API-test executable imports for
OS input, cursor/focus setters, window input posting and hooks. It does not claim
that import inspection proves all dynamically resolved behavior absent.

Desktop-wide cursor/foreground observations are diagnostic: user activity is not
blocked. Initial new test asserted interval-wide foreground equality and failed
while the desktop changed; it now reports that observation separately from API
correctness and prohibited-import checks. No clipping or activation is added to
force a pass. Legacy GUI/SDL native-baseline tests themselves intentionally use
physical input/foreground/cursor for their controller/test fixtures, not as HVI
delivery. Full-suite runs have recorded desktop-sensitive failures; report those
alongside successful core tests, not as generic application compatibility.

## Prioritized remaining work

1. Negotiate runtime/adaptor capabilities and protocol versions; expose explicit
   Unsupported instead of silently substituting another delivery mechanism.
2. Add application-facing Win32 facade with documented local/screen coordinate
   mapping, GetKeyState semantics, mouse VK policy, focus policy and independent
   consumer cursors. Do not claim OS queues or native input were virtualized.
3. Complete controlled DirectInput factory/enumeration/format/acquisition contract
   and SDL mapping/multi-window/text/warp coverage with negative tests.
4. Add explicit input leases, per-consumer diagnostics, overflow/backpressure
   policy and bounded numeric accumulation. Keep reset/release semantics distinct.
5. Design an application-authorized attachment contract. A DLL sitting beside an
   arbitrary EXE or a named pipe alone cannot redirect that EXE's input API calls.

## Concrete EVE plan and gate

1. Preserve existing passive observations and operator-confirmed keyboard
   PostMessage result as separate evidence. Neither proves that EVE calls an HVI
   adapter, nor identifies its physical keyboard/mouse acquisition path.
2. Establish a permitted integration surface (documented client/plugin/SDK or
   vendor-approved test build). Verify architecture/ABI and which input API is
   actually exercised using permitted diagnostics. DLL presence is insufficient.
3. Reproduce that API contract in the existing controlled Win32/DI/SDL targets.
   Cover foreground/background, state + buffered events, acquire/unacquire,
   focus changes, DPI/coordinates, reset/disconnect and independent two processes.
4. Only if a permitted surface exists, load/bind Runtime/adapters through it and
   run explicitly operator-controlled A/B tests with app-visible results. Verify
   API consumption, not IPC ACKs. No client edits, injection, hooks, protection
   bypass, fake Raw Input handles or hidden PostMessage fallback.
5. If no such surface exists, stop at the controlled compatibility layer and
   document EVE attachment as BLOCKED. No supported attachment is established
   today. DirectInput/Raw Input/Win32 execution paths, sanctioned loader and EVE
   compatibility with HVI Runtime all remain UNCONFIRMED.

References: Microsoft Learn GetKeyState (queue-associated state), GetAsyncKeyState,
GetNamedPipeServerProcessId and DirectInput buffered/immediate data contracts;
pinned official SDL source under E:\python\HVI\build\_deps\sdl2-src\src\dynapi.