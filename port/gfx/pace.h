#ifndef MLFK_GFX_PACE_H
#define MLFK_GFX_PACE_H
// port/gfx/pace.h — the frame-pacing wait, SHARED by every paced app.
//
// Lifted out of the two byte-identical `static void sleep_until_ns` copies
// in port/gfx/gfx_app.c and port/foh/foh_dev.c (M4 task 14 increment 3e),
// following the attrib.h precedent from increment 3a: one seam, one owner,
// so a pacing change cannot land in only one of the two binaries the M4
// gate judges (CLAUDE.md HARD RULE 8 — class fix over a second copy).
//
// WHY IT IS HYBRID AND NOT A BARE nanosleep LOOP
// ----------------------------------------------
// The old body slept the WHOLE remainder and returned whenever the kernel
// got around to running us again. Measured on the device
// (12c00003237f5528, kernel 4.14.14-funkey, single core):
//
//   - /proc/timer_list reports .resolution 1 nsecs and hrtimer_interrupt,
//     /proc/self/timerslack_ns is 50000, there is no cpufreq sysfs and no
//     cpuidle state. So the TIMER is not the problem: the modal frame
//     start deviation is 85-135 us, which is timer slack plus loop tail.
//   - The tail is not. Pooled over the iter-110 (174 rows) and iter-113
//     run-8 (115 rows) attribution corpora, 34 frames started 1.24-5.16 ms
//     late with the previous frame INSIDE its budget — i.e. the delay
//     accrued while this thread sat in nanosleep, waiting to be run again
//     on a core it shares with the audio callback and the music reader.
//     Two of those collided with a heavy sim tail and skipped a render,
//     which is what fails the M4 gate.
//
// A thread that is already spinning does not have to be woken and
// selected before it can observe its deadline. So: sleep coarsely to
// (target - PACE_SPIN_NS), then poll CLOCK_MONOTONIC to the deadline.
//
// The wait is NEVER shortened — `now >= target` guards the only return,
// so this is a strictly more accurate implementation of the same
// contract, and pacing is the only thing it can affect. No sim, stream,
// checksum or judged-timing surface reads it.
//
// PACE_SPIN_NS = 3 ms is a calibration knob, derived in
// .loop/m4-t114-prereg.md §3: the largest window that stays clear of the
// low end of the device's measured 26-48% idle band (a spin costs
// S/16.667 of wall time that used to be sleep, and the audio callback is
// a sibling thread on the same core), while carrying 2.1x margin over the
// 1.4 ms that the two measured skips actually required.
//
// NOT USED BY port/sim/device/skip-attrib/sk_sampler.c: that one paces a
// 250 ms diagnostic sample cadence, not a frame. Spinning 3 ms out of
// every 250 ms would be waste, and the sampler is decision-inert. It
// keeps its own bare loop deliberately — different class, different seam.

#include <stdint.h>
#include <time.h>

#include "attrib.h" // attrib_mono_ns(): the ONE CLOCK_MONOTONIC read both
                    // TUs already share. Each TU's private now_ns() is
                    // byte-identical to it, so this adds no third clock.

// The spin window. Overridable at compile time so the knob can be turned
// without touching this file; see the derivation + escalation table in
// .loop/m4-t114-prereg.md §3 (4 ms, then 5 ms, judged on underruns first).
#ifndef PACE_SPIN_NS
#define PACE_SPIN_NS 3000000ull
#endif

// SMALL PACED BUDGETS ARE SAFE — do not add a budget floor here.
// review-114 round 1 argued a paced budget <= PACE_SPIN_NS would make
// every wait a pure busy-poll, and a floor was briefly added. Round 4
// showed that is a hypothetical with NO in-tree instance, and that the
// floor BROKE two committed standing tests which deliberately pass
// `--pace 1 --budget-ns 1000`: the frameskip-valve tooth
// (port/gfx/check-device-render.sh:806, asserts 119 skipped / 1 rendered)
// and the frozen-since-iter-57 T5 starvation probe
// (port/gfx/check-device-audio.sh:157). Both are safe because a 1000 ns
// budget puts every deadline in the PAST, so the `now >= target` guard
// returns before `rem` is ever computed and no spin occurs. A busy-poll
// needs budget <= PACE_SPIN_NS *and* frame work shorter than the budget;
// nothing in this tree runs a frame loop that fast. The floor was
// reverted.

// Wait until CLOCK_MONOTONIC reaches `target`.
//
// Returns only when the clock has actually reached the deadline: a target
// already in the past returns immediately (unchanged from the bare loop —
// this is what makes the change a provable no-op on frames that already
// overran, rather than a behaviour change on them).
//
// The spin phase is bounded by PACE_SPIN_NS by construction: it is only
// reachable once `rem <= PACE_SPIN_NS`, and `rem` shrinks monotonically.
// EINTR needs no handling: the remainder is re-derived from the clock on
// every iteration, exactly as the bare loop did.
static inline void pace_sleep_until_ns(uint64_t target) {
  for (;;) {
    const uint64_t now = attrib_mono_ns();
    if (now >= target) return; // the only exit — never early
    const uint64_t rem = target - now;
    if (rem <= PACE_SPIN_NS) continue; // spin phase: re-read the clock
    uint64_t nap = rem - PACE_SPIN_NS;       // coarse phase
    // Cap one nap at 1 s. Raised by review-114 round 1 as a narrowing bug
    // on the assumption that armv7 time_t is signed 32-bit; MEASURED on
    // the actual toolchain (jondbell/funkey-s-sdk, gcc 10.2, musl 1.2+):
    //   sizeof(time_t) == 8, signed;  sizeof(tv_nsec) == 4
    // so tv_sec CANNOT narrow here and that specific bug is not reachable
    // on this target. The cap is kept anyway because it is one comparison
    // and it makes the shared header correct on a 32-bit-time_t target
    // too: with it, tv_sec is only ever 0 or 1 and tv_nsec is always in
    // range, so EINVAL is unreachable by construction and the only
    // possible error is EINTR, which the re-derived remainder already
    // handles -- which is why the return value is deliberately ignored.
    // A longer wait simply naps again. (Recorded as measured-not-inferred
    // per the same discipline that corrected the ruling's HZ premise.)
    if (nap > 1000000000ull) nap = 1000000000ull;
    struct timespec ts;
    ts.tv_sec = (time_t)(nap / 1000000000ull);
    ts.tv_nsec = (long)(nap % 1000000000ull);
    nanosleep(&ts, 0);
  }
}

#endif // MLFK_GFX_PACE_H
