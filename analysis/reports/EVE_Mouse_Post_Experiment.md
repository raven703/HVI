# Independent EVE mouse PostMessage experiment

2026-11-21. Extends E:\python\HVI\experiments\eve_key_post.cpp; same executable
and append-only log. No Hunter/EVE changes, hooks, injection, cursor movement,
capture, activation, or SendInput. Delivery to EVE is operator-controlled only.

## Launch and manual background test

```powershell
& 'E:\python\HVI\build\eve_key_post.exe'
```

Start/restore both EVE clients manually, then leave the console foreground.
`list` shows candidate HWNDs, client dimensions, and per-HWND virtual state.
At inspection on this stage no matching EVE windows were found; use the fresh
inventory after launching the clients. No messages were sent to EVE by tests.

Choose a harmless UI element and coordinates inside each window's client area
(origin is top-left of content, not the title bar). Example positions below are
illustrative; replace with your own safe coordinates, not gameplay targets:

```text
select A
pos 100 100
move
confirm yes
select B
pos 200 150
move
confirm yes
select A
lclick
confirm yes
select B
rclick
confirm yes
wheel -120
confirm yes
wheel 120
confirm yes
select BOTH
move
confirm yes
release
quit
```

Use yes/no/unknown according to actual visual response, not QUEUED. Confirm
immediately after the operation. Inspect the non-selected window as well.
Repeat left/right/wheel on each recipient as desired. BOTH retains different
positions: A uses 100,100 and B 200,150 in this example. BOTH is sequential posts,
not atomic simultaneous input. `pos` changes state only; it sends nothing.

Explicit HWND selection: `select hwnd:200488` or `select hwnd:0x30F28`
(examples only; decimal/hex values must match a currently discovered candidate).
`select A`, `select B`, and numbered selection still work.

Drag/hold: `ldown`, `pos X Y`, `move`, `lup`; right equivalent is `rdown`/`rup`.
Positions/buttons belong to HWNDs, not a shared mouse. `release` attempts all
tracked keyboard and mouse ups, even for previously selected recipients. It
retains failed holds for retry. Normal quit/EOF releases; crash/forced termination
does not guarantee release. No auto-repeat or real mouse capture is simulated.

## Encoding / diagnostics

- WM_MOUSEMOVE and WM_LBUTTONDOWN/UP, WM_RBUTTONDOWN/UP: signed 16-bit client
  coordinates in lParam; wParam contains the recipient's virtual MK_LBUTTON /
  MK_RBUTTON state *after* the button transition.
- WM_MOUSEWHEEL: converts the virtual client point to screen coordinates for
  lParam on each send. Signed delta goes in high word of wParam; +/-120 is one
  notch. Physical keyboard modifier state is not copied into mouse messages.
- Coordinates must be inside the current client rectangle for position/move/
  downs/wheel and fit signed 16-bit message fields. Ups permit saved positions
  after resize so a tracked hold can still be released. Screen overflow fails.
- No cursor-position setter or foreground setter is used. Cursor and foreground
  are sampled before/after each send. External changes between samples are not
  blocked; samples are observations, not a continuous trace.
- Every MOUSE_POST records PID/HWND, message, client and encoded coordinates,
  wParam/lParam, wheel delta, queue result, numeric Win32 error, cursor samples,
  foreground samples, target_foreground and game_processing=UNKNOWN.
- HWND/PID/TID/class/process-name validation is retained. UIPI errors are
  reported without elevation/bypass. QUEUED is not evidence of game processing.
- confirm logs last attempted action plus current selection/positions; it is a
  manual operator report, not an automatic response detector.

## Automated validation

The existing `eve_key_post_owned` test now covers keyboard regression plus mouse
A/B/BOTH delivery/isolation, left/right order and masks, independent coordinates,
held-button move masks, all-button release, positive/negative wheel deltas and
screen-coordinate conversion, signed packing, bounds/overflow, stale HWND,
and unchanged foreground with hidden inactive owned recipients.
Cursor interval movement is diagnostic, not a failure: a person may move their
physical mouse during the test. Per-post samples and executable import inspection
check that the sender does not request movement. Initial runs reached this
interval assertion and failed due to observed cursor movement; that assertion
was replaced with an explicit diagnostic, not a claim that the cursor stayed still.
EVE mouse reactions remain for manual observation.

Win32 reference: Microsoft Learn WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_RBUTTONUP,
WM_MOUSEWHEEL, PostMessageW. In particular wheel lParam is screen-relative,
unlike client-relative button/move lParam. No WM_SETCURSOR/WM_MOUSEACTIVATE or
WM_CHAR messages are explicitly posted and no physical input is generated.