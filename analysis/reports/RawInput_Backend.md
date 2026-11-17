# Raw Input backend

PARTIAL native receiving wrapper: RegisterRawInputDevices for keyboard/mouse INPUTSINK, unregister, single-handle GetRawInputData, GetRawInputBuffer, GetRawInputDeviceInfoW. Caller must use actual lParam from WM_INPUT; wrapper does not own message loop or silently register during DLL load. Only one recipient per usage class per process; registration is explicit. HID variable packets beyond fixed RAWINPUT storage are not supported by read().

CONFIRMED by test: native registration/query/unregistration, invalid native handle rejection; own synthetic RAWKEYBOARD/RAWMOUSE codec basic key break/make and relative motion. SyntheticRawPacket is explicitly tagged with session, NOT an HRAWINPUT; not posted as WM_INPUT and never treated as kernel input. Device handle is null in synthetic records.

UNSUPPORTED: creation of genuine per-session HRAWINPUT handles from HVI commands, virtual HID hardware, native Raw Input injection. Do not infer injection support from serialization or PostMessage(WM_INPUT). No user32 Raw Input hooks installed. Synthetic codec is only a format experiment, not native buffered input support.

UNKNOWN: real hardware WM_INPUT delivery and native buffered/device-info behavior on this machine; tests do not manufacture physical input. Multiple HVI sessions are distinguished only in explicitly synthetic records.

Primary references: Microsoft Learn Raw Input Overview; GetRawInputData; GetRawInputBuffer; WM_INPUT. GetRawInputBuffer consumes native queued packets/messages; ordinary pointers do not acquire those semantics. Documentation consulted 2026-11-17.