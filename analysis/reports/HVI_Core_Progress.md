# HVI core development — 2026-11-17

## Working and verified
- Backward-compatible 28-byte event layout and existing kinds; Wheel=8 and Focus=9 added.
- Keyboard: held state, per-frame press/release latches, repeated-down count without duplicate buffered edges, Caps/Num/Scroll toggle bits, async press latch consumed on query. Left/right Shift/Ctrl/Alt aggregate into generic modifier. Generic modifier commands normalize to left variant.
- Mouse: five buttons, signed 64-bit logical absolute position, accumulated relative deltas, vertical/horizontal wheel. Absolute updates emit delta records for the event model; no desktop normalization or OS cursor movement is implied.
- Ordered timestamped transition records: global, keyboard and mouse queues independently bounded at 256 records. Drop-oldest overflow flag, peek, partial read and drain. Reset/Stop release held keys/buttons and clear logical mouse/toggles; disconnect discards state and queues.
- State is thread-confined; InputDevice serializes apply/snapshot/query through mutex. Existing router and transport remain synchronous, not thread-safe shared objects. Separate per-session transports can operate concurrently.
- DeviceAdapter capability/snapshot/buffer/lifecycle interface and CooperativeAdapter reference implementation allow future backends without changing state model. Capabilities explicitly deny native OS input synthesis.
- api_probe now substitutes GetDeviceData on its owned DirectInput keyboard object, translates events to DIDEVICEOBJECTDATA, implements bounded count/peek/drain and returns DI_BUFFEROVERFLOW. Every sample compares peeked/consumed sequence and verifies drain. This is a controlled fixed-format adapter, not complete DirectInput emulation.
- Console test_controller launches 2–16 probes, chooses target session, sends key/button/move/absolute/wheel/reset/snapshot commands, prints remote API observations and held key/button lists from receiver logs, owns and cleans up child processes. No local predicted-state display is presented as a remote result.

## Executed tests
- Existing state/routing and model tests: zero errors.
- tests/extended_input.py: two concurrently commanded processes, A/B independence, left/right Shift, hold/release, absolute/relative movement, wheel/button, reset, buffered peek/drain PASS.
- tests/api_isolation.py: existing two-background-process API-path isolation PASS.
- tests/integration.py: receiver death isolation, reconnect reset, restart PASS.
- tests/focus_input.py: actual foreground gain AND loss observed; held key persisted and all substituted API samples agreed; PASS in current interactive desktop. Test explicitly requests focus only for this scenario. OS foreground policy can make later runs inconclusive, recorded separately rather than silently passed.
- Controller smoke: launched two processes, targeted A and B independently, mouse/wheel commands and remote logs PASS.

## Experimental / incomplete
Not production-stable IPC: synchronous named-pipe read/write can block indefinitely on a connected stalled peer. Default pipe security and no client authentication; trusted local test environment only. GUI pump still waits between commands. Failure recovery is covered by legacy receivers, not reconnectable api_probe/controller sessions; controller reports failure and cleans up rather than reconnecting automatically.

Mouse buffering is implemented in the core model, not as a native DirectInput mouse COM adapter. Keyboard DirectInput shim assumes fixed c_dfDIKeyboard; no emulated SetProperty buffer-size, SetDataFormat negotiation, Acquire/Unacquire state machine, notification handles or full scan-code/layout fidelity. Async low-bit consumption is deterministic HVI behavior, not a claim to reproduce Windows' cross-process historical semantics. Model tests cover toggles; actual system toggle state is intentionally not modified. Transition latches require nextFrame; mouse deltas accumulate until reset in this iteration. No callbacks run under internal locks; consumers poll buffers. Probe adapters and state run on one thread.

HVI virtual state is not native Windows input. Substitution affects only owned probe executable import sites and owned COM object. No arbitrary-app/game support, injection, drivers or VM independence claim beyond tested cooperating processes. Main Hunter and reference binaries unchanged.

## Run
Build CMake target test_controller; run C:\Personal\HVI\HunterVirtualInput\build\test_controller.exe 2.
Commands: target 1; down 65; up 65; button 0 1; move 10 -5; absolute 200 300; wheel 120 0; state; reset; quit. Each command occupies its own line. Sessions use existing pipe IDs 1..N; stop conflicting demos before launch.

Automated scripts: tests/extended_input.py, tests/focus_input.py plus existing api_isolation.py and integration.py. CTest includes model and legacy unit tests; desktop integration scripts remain explicit because focus tests require interactive desktop access.