# EVE PostMessage keyboard experiment: measured result

Source: E:\python\HVI\build\eve_key_post.log, 4732 bytes at inspection.
Operator session started 2026-11-21 08:15:49.522 UTC. Earlier 08:07 sessions
are discovery/CLI checks with no keyboard posts. No input was sent during this
log review. The operator separately reported that everything worked visually.

## Observed trials

| Trial | Target PID / HWND | Down UTC | Up UTC | Evidence of visible response |
|---|---|---|---|---|
| ESC A | 4888 / 200488 | 08:17:33.383 | 08:17:33.492 | manual yes at 08:17:50.489 |
| ESC B | 21584 / 3477462 | 08:18:06.173 | 08:18:06.280 | manual yes at 08:18:22.233 |
| ESC BOTH, A half | 4888 / 200488 | 08:18:45.257 | 08:18:45.360 | manual yes for BOTH at 08:18:57.856 |
| ESC BOTH, B half | 21584 / 3477462 | 08:18:45.266 | 08:18:45.369 | same BOTH confirmation |
| W hold A | 4888 / 200488 | 08:19:15.858 | 08:19:19.257 | overall operator report; no per-trial confirm entry |

All **10 posts** returned QUEUED, Win32 error 0: five downs and five ups.
Each recorded down has a later matching up; no unmatched posted-down keys in
this inspected log. W down/up separation is 3.399s; that is not proof of
continuous game-side movement/held-state processing.

Every post records foreground_before=266132 and foreground_after=266132.
That HWND differs from both target HWNDs. Thus no foreground change is observed
at send boundaries, and neither target HWND was foreground at those boundaries.
This is not continuous foreground tracing and cannot exclude intermediate
changes between records. Sender source contains no target activation request.

Release All entry at 08:19:28.400 UTC has no subsequent key-up posts in the
inspected log; all logged downs had already been paired with ups. This is a
no-held-key release case, not an EVE test of releasing outstanding holds.
There is no distinct quit/session-end record; do not infer clean process exit.

## What is supported by the evidence

- Queueing: CONFIRMED successful PostMessage calls for these targets/keys.
- ESC visible response in A, B, BOTH: OPERATOR-CONFIRMED, with three explicit
  yes confirmations. This supports a usable posted-keyboard-message path for
  the tested ESC UI action under this session's conditions.
- Operator overall visual success is recorded separately from API return
  values. No pixels/game-state oracle was collected.
- W Down/Up A: queueing confirmed and overall visual success reported; exact
  game-side effect and continuous hold semantics were not separately recorded.
- No target messages to B were logged during the A-only trial, or to A during
  B-only. No explicit per-trial observation of the *non-selected* client is
  present, so absence of cross-client side effects is not independently verified.

## Still not established

- A and S were not posted in this log; W B/BOTH was not tested.
- Release All with outstanding keys in EVE was not tested.
- No mouse delivery experiment was performed here.
- Physical-input handling, keyboard-state polling, Raw Input and DirectInput
  use remain UNKNOWN. Successful ESC message handling does not prove that all
  keyboard/gameplay paths use Win32 messages or exclude other mechanisms.
- This standalone experiment is not yet integrated into HVI and does not
  establish generic focus-independent game input or arbitrary-key compatibility.

## Next bounded experiment

In a safe UI context, test A/W/S individually on A, B, BOTH; explicitly record
each selected client's expected response and whether the non-selected client
stayed unchanged. Separately test a tracked down followed by Release All while
the console remains foreground. Do not use gameplay commands or infer a backend
from lack of response. Mouse is a separate experiment with its own coordinate,
button-state and response oracle requirements, not a conclusion from ESC.

No Hunter/EVE modifications, DLL injection, hooks, memory access, protection
bypass or additional target input during this analysis.