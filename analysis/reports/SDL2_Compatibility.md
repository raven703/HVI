# SDL2 compatibility experiment

## Version and provenance

Official SDL2 2.30.11, revision SDL-release-2.30.11-0-gfa24d868a, built from unmodified source. SHA256 of official zip: a0b3e7ac5f708042683ff0f22e069bdf75563540c615f9854ecc9bc8913e2488. CMake FetchContent pins archive/hash. The test object's poll/update/render pattern follows public SDL examples; it includes only SDL and standard C++ headers, has no HVI imports, headers, pipes, session ID or input command channel.

Reference documents: SDL2 Wiki README-dynapi, SDL_PollEvent, SDL_GetKeyboardState. Relevant pinned source: src/dynapi/SDL_dynapi.c and SDL_dynapi_procs.h; src/video/windows/SDL_windowsevents.c and SDL_windowsmouse.c. The generated dynapi table header is consumed unmodified from the pinned source, preserving ABI order and its license notice.

## Actual APIs

Test object calls SDL_PollEvent, SDL_GetKeyboardState, SDL_GetMouseState or SDL_GetRelativeMouseState, SDL_SetRelativeMouseMode. Native normal-mode trace observed GetKeyboardState, GetAsyncKeyState, GetKeyState and dispatched keyboard/mouse messages. DirectInput8 module was not loaded in the recorded test. This is observation of this video-only initialization, not proof that SDL never uses DirectInput (joystick/haptic backends differ).

| API | Used by pinned native backend / observation | Focus | HVI compatibility |
|---|---|---|---|
| Win32 messages | CONFIRMED key/mouse message counters in normal-mode run | ordinary key delivery follows real foreground | Not virtualized at Win32 level; SDL dispatch override instead |
| Win32 keyboard state | CONFIRMED GetKeyboardState/GetAsyncKeyState/GetKeyState counters | native OS/thread rules unchanged | Existing Runtime virtual exports separate; no generic Win32 interception here |
| DirectInput | No dinput8 loaded in observed video-only run; keyboard/mouse via other paths | N/A for this test | Runtime A/W factories exist; native DI8Create interception UNSUPPORTED |
| Raw Input | Relative mouse path identified in pinned source; separate relative trace collected | native SDL capture/focus rules | Native HRAWINPUT virtualization UNSUPPORTED |
| SDL dynapi | CONFIRMED B_SDL_DYNAPI and real object reactions | HVI held state independent of foreground | Selected SDL poll/state/relative-mode functions only |

## Status

CONFIRMED: two independent background squares, crosshair reaction, color/scale isolation, BOTH/NONE, reconnect, native restoration in automated interactive run. TRACE_NATIVE is instrumentation only; B_SDL_DYNAPI initializes Runtime via compatibility module, not via test EXE. Existing HVI GUI clients are mode A cooperative evidence, never counted as B.

PARTIAL: snapshot sampling can miss rapid down/up between polls; synthetic event timestamps are observation time. Only tested SDL API subset and a single window/main input thread per process. Mouse virtual coordinates are currently 640x480 scoped. International VK translation, SDL text/IME, event watchers/Peep/WaitEvent and global mouse APIs are not virtualized. PollEvent(nullptr) does not remove pending virtual events. Full SDL ABI/table is pinned, not a cross-version guarantee.

UNSUPPORTED: fake HRAWINPUT, OS-wide cursor virtualization, arbitrary Win32/DirectInput application integration, injection into remote/protected processes.

UNKNOWN: compatibility with pre-existing third-party SDL applications, SDL3, static builds without dynapi, mixed DPI/multiwindow input, broader thread safety. Successful local backend experiment cannot be transferred to applications using different APIs without tests.