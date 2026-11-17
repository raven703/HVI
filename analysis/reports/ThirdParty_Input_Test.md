# Independent application input experiment

## Run

Build all targets; run build/hvi_sdl_demo.exe. Close old HVI GUI clients first because they also own sessions 1/2. SDL Test A/B and existing HVI Control appear. Select target, enable Hold/toggle, hold W in A and D in B while Control is foreground. Mouse pad moves selected crosshair; LMB toggles color; RMB toggles movement direction; wheel changes scale. Release All clears holds. Foreground an SDL window and press F12 to switch only it to native input. R toggles relative mouse mode; ESC closes. Native standalone: build/sdl_input_test.exe (no launcher, no HVI environment).

## Observation protocol

The independent application optionally writes JSON via --observe path. It never reads that file or accepts input commands through it. Coordinates, scale, color, actual SDL-held keys, focus and frame/event counters come from its update loop. Tests tolerate partial reads because the file is rewritten. Controller commands still go through Runtime IPC; JSON cannot supply commands.

CONFIRMED: automated interactive checks observe real square movement A-only, independent simultaneous movement, crosshair isolation, A-only color, B-only scale, BOTH/NONE, background state, disconnect/reconnect, B surviving A closure and native physical W movement before/after virtualization. Virtual commands do not call SetCursorPos; test itself moves/restores cursor to exercise real controller pad. Evidence: build/sdl_compatibility_result.json; committed snapshot SDL2_Compatibility_Result.json.

PARTIAL: input/backend behavior validated on this newly built independent SDL test object, not an unrelated published game. Read-only object state proves update-loop reaction; no pixel-image oracle used. Some focus activation in automation uses native Alt + SetForegroundWindow; tests fail if desktop policy refuses. Short clicks can be lost by snapshot bridge; use hold mode for reproducible state checks.

UNSUPPORTED: claims that successful IPC is third-party compatibility; remote/protected process injection; native Raw Input synthesis.

UNKNOWN: unrelated apps, EVE, anti-cheat environments, other input backends. Existing cooperative stand/tests preserved, but do not count towards SDL compatibility proof.