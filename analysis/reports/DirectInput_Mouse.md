# DirectInput mouse experiment

CONFIRMED: x64 unit tests exercise typed IDirectInputDevice8A and W implementations, canonical DIMOUSESTATE/2 formats, consuming relative X/Y/Z polls, five buttons in extended format, buffer capacity 0..256, peek/drain, overflow, acquire/unacquire, QueryInterface/AddRef/Release and reset. CTest: directinput_mouse_unit.

PARTIAL: only canonical c_dfDIMouse/c_dfDIMouse2 formats, relative axis mode, nonexclusive/background cooperative request accepted. Basic format exposes four buttons as required by its layout. Each COM object has its own immediate/buffer cursor; lifetime shares owned feed, not a raw context pointer. Methods not implemented explicitly return DIERR_UNSUPPORTED. No legacy COM interface IDs beyond IUnknown and matching Device8 A or W are advertised. Foreground/exclusive modes unsupported. Buffer capacity above 256 rejected, not silently accepted.

UNKNOWN: third-party compatibility; x86; full focus/cooperative semantics. Two-process runtime mouse test will be recorded separately, not inferred from unit results. No claim of a complete replacement for native DirectInput.