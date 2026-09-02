// port/foh/foh_crashlog.h — when the app dies of a signal, say WHERE.
//
// WHY. The deliberate failures in this codebase already explain themselves:
// sim_fatal and gfx_fatal print a sentence naming the rule that refused, and
// the launcher copies the log to the SD card on exit, so those arrive
// diagnosable. A genuine SIGSEGV does not. The process vanishes, the log stops
// mid-line, and the only evidence is `RC=139` in opk.rc — which says "it
// crashed" and nothing else.
//
// That is the wrong way round: the crashes that explain themselves are the
// ones somebody predicted, and the ones that matter are the ones nobody did.
// MEASURED, 2026-09-01: an owner-reported crash (pause -> quit, after a
// resume) left no evidence at all, and diagnosing it from source alone
// produced three wrong theories in one session.
//
// WHAT IT WRITES. One line, appended to a file on the SD CARD — not /tmp,
// which is tmpfs and is erased by exactly the power cycle a crash tends to be
// followed by:
//
//   CRASH sig=11 code=1 addr=0x... pc=0x... bin=<sha16> t=<uptime-ish>
//
// With `bin`, a PC is enough to name the function: the build is reproducible,
// so `addr2line -e foh_device <pc>` against the matching binary gives file and
// line. Nothing here tries to symbolise on the device.
//
// WHAT IT DOES NOT DO, deliberately. No backtrace walk, no malloc, no printf.
// A signal handler for a fault runs on a corrupted process and may be on a
// damaged stack; anything clever is a second crash on top of the first, and
// then there is no evidence at all. This uses only async-signal-safe calls
// (open/write/close), formats into a fixed stack buffer by hand, re-raises
// with the default handler so the exit status still reports the signal, and
// is otherwise as boring as it can be made.
#ifndef FOH_CRASHLOG_H
#define FOH_CRASHLOG_H

// Install handlers for SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT. `dir` is where
// the line is appended (the persist dir — the SD card on the device); `binsha`
// is the 16-char build prefix the launcher already logs, so a crash line can
// be matched to the binary that produced it. Safe to call once, early.
void foh_crashlog_install(const char *dir, const char *binsha);

#endif
