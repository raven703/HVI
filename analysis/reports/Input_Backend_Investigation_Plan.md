# Input backend investigation (no EVE attachment)

2026-11-21 status: standalone **passive** inspector is implemented and validated;
see Input_Backend_Inspector.md and EVE_Input_Investigation.md. The historical
"UNSUPPORTED standalone inspector" note below applies to API execution tracing,
not the new snapshot/foreground observer. API-level backend identification still
remains UNKNOWN. No invasive tracing, injection or protection bypass is planned.

CONFIRMED: existing owned-SDL compat_trace instrument counts selected Win32 imports and messages. It is not a separate universal inspection program and must not be loaded into EVE. No EVE process access, injection, hooks or security bypass performed.

Required observations before backend selection:
1. Obtain authorized documented/source-level evidence of input backend and exact module versions; static imports indicate possibilities, not runtime calls.
2. On an owned harness, measure Win32 message dispatch, keyboard-state calls, DirectInput8Create and matching A/W device interfaces/state/data calls, acquisition/cooperative flags, Raw Input registration/read modes and focus/capture.
3. Record foreground/background behavior, pause-on-blur policy, relative/raw mode, architecture and initialization/shutdown paths.
4. Choose backend only from verified call paths: SDL dynapi is SDL-specific; DI factories alone do not intercept DI8Create; native Raw Input virtualization remains unsupported.

PARTIAL: existing SDL diagnostics do not instrument dynamic GetProcAddress DirectInput calls or COM vtables and cannot prove absence. UNKNOWN: EVE's actual mechanisms and policy. UNSUPPORTED in this delivery: standalone DirectInput/backend inspector; EVE compatibility inference. Build a separate owned Win32/DI diagnostic harness before any further authorized application study.