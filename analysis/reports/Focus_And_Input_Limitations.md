# Focus and input boundaries

CONFIRMED: virtual held input moves SDL objects while Control is foreground and both SDL observers report focus=false. Native baseline requires actual foreground granted by Windows. Compatibility does not forge window focus flags. Closing A leaves B alive. F12 is checked only for foreground PID, so one process toggles without disabling the other.

PARTIAL: only selected SDL input functions are replaced. SDL window/lifecycle focus events remain native; this test's movement loop intentionally polls SDL state regardless of focus. An arbitrary app may refuse to update when unfocused, pause simulation, or ignore input; this module does not override such policy. Normal/relative virtual mouse uses private coordinates/deltas and no OS cursor warp. Native relative mode may legitimately capture/clip/warp cursor after virtualization is disabled.

PARTIAL: disconnect resets holds. Mouse Reset can create a baseline discontinuity in snapshot adaptation; universal event-fidelity/reset semantics require transition-based bridge. Keyboard rapid clicks between snapshots are not guaranteed. Queued event paths other than SDL_PollEvent remain native; do not mix them expecting isolation. No IME/text input, multiwindow, thread-safe general SDL wrapper or complete modifier/keycode coverage is claimed.

UNSUPPORTED: bypassing Windows focus/security restrictions, injecting into protected/unowned processes, blocking physical user input globally, native hardware Raw Input virtualization.

UNKNOWN: multi-account desktop, locked/headless GUI session, mixed-DPI and multi-monitor usability, arbitrary third-party focus behavior. GUI tests require interactive desktop and temporarily send physical W/F12 and move cursor; don't use during unrelated work.