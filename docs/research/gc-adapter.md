# GameCube adapter over the FunKey-S micro-USB port — feasibility spike (A33)

Research ticket A33 (`fix_plan.md:4175`), lane R. Question: can the official
Wii U / GameCube controller adapter (or any USB controller) be supported on
the FunKey-S through its micro-USB port?

**VERDICT: UNKNOWN — and the unknown is small, cheap and electrical.**

Everything I originally called a blocker turned out to be software living in
trees the owner already forks and builds (DrUm78's FunKey-OS and kernel).
Those are three roughly one-line edits, not a research programme. What is
genuinely not answerable from source is a single electrical question: **can
USB0's PHY drive a host bus on this board, given VBUS is an input rail and
the ID pin is unpopulated?** One free command on a reconnected device gets
most of the way to an answer (§7). Nobody should spend days on this until
that command has been run — and nobody should abandon it before then either.

- Device: FunKey-S running **DrUm78's custom FunKey-OS** (the owner's actual
  baseline), kernel **4.14.14** from `DrUm78/linux` @ `v1.0.3-funkey-s`,
  Allwinner V3s, ADB id `12c00003237f5528`.
- **No device runs were performed** — the device is disconnected for this
  lane (lane rule 3). Every claim is from primary source text: the DrUm78 and
  FunKey-Project OS/kernel repositories, FunKey vendor documentation, and the
  owner-provided GitHub projects, pinned by path, tag and line.
- One owner-provided source could not be retrieved (§6).

---

## 0. Corrections — two, both from owner challenges in-session

Recorded in place rather than silently edited, so the reasoning stays
auditable. Both corrections moved the verdict.

**C1 — power was never a kill.** The first draft called power "a second
independent kill" and told the reader to buy a self-powered USB hub. The
owner pointed out the official adapter has **two** USB plugs, the grey one
power-only for rumble. So the ticket's 500 mA is not a host-side obligation
and no hub is involved. See §2. *Lesson: I reached for a second kill before
the first one needed help, and got the peripheral's physics wrong.*

**C2 — I made a scope decision that was not mine, and it was the load-bearing
one.** The draft asserted that changing the kernel "means rebuilding and
reflashing FunKey-OS — an image this project neither owns nor ships. We ship
an OPK." That sentence did the real work of the NO-GO, and **it was my
assumption, not a finding.** The owner already runs DrUm78's custom FunKey-OS
and can fork it. With that corrected the NO-GO collapses, because every
remaining software obstacle is a line in a tree the owner already builds.
*Lesson: I converted "I don't know the project's appetite for this" into "the
project won't do this" and shipped it as a verdict. Scope is the owner's
call — state the cost, let them price it.*

---

## 1. Q1 — Does the micro-USB port do USB host? **Not as shipped. Tractable in a fork; one electrical unknown remains.**

### 1.1 As shipped, three things say "peripheral" (CITED)

Verified against **DrUm78's tree**, the owner's real baseline — which is
byte-identical to upstream FunKey-Project in the USB region, so the earlier
upstream reading holds:

| # | Fact | Source |
|---|---|---|
| 1 | `dr_mode = "peripheral"` on the OTG controller | `DrUm78/linux` @ `v1.0.3-funkey-s`, `arch/arm/boot/dts/sun8i-v3s-funkey.dts:251-254` |
| 2 | `CONFIG_USB_MUSB_GADGET=y`, no `HOST`, no `DUAL_ROLE` | `DrUm78/FunKey-OS` @ `FunKey-OS-DrUm78`, `FunKey/board/funkey/linux.config:115` |
| 3 | `# CONFIG_HID is not set` / `# CONFIG_USB_HID is not set` | same file, `:107-108` |

In Linux 4.14 the three MUSB roles are a **mutually exclusive Kconfig choice**
(`drivers/usb/musb/Kconfig`), so as built, host-side MUSB code is not merely
disabled — it is not compiled. The `&usbphy` node (`:256-260`) declares no
ID-detect GPIO, so the running kernel performs no role detection at all.

Consistent with observed behaviour: the `adb` marker file at SD root starts
adbd over USB (`CLAUDE.md`, Device access), and adbd is a FunctionFS gadget
(`CONFIG_USB_FUNCTIONFS=m`, `:123`).

### 1.2 The ID pin is a red herring for a forced-host build

FunKey vendor hardware reference, *USB*
(`doc.funkey-project.com/developer_guide/hardware_reference/usb/`, fetched
2026-08-23) — the port carries +5 V, GND, D+ and D−, and on the ID net:

> "The resistor **R4** on the USB ID connector pin should probably not be
> mounted" — "as we act only as an USB device, this pin should be left
> floating."

…because "we don't need the ID pin to determine by the cable wiring which
role we must take." The same page states outright that "the V3s is able to
work as either an USB host or USB device using the USB OTG protocol."

**The first draft treated this as the decisive hardware kill. That was too
strong.** The ID pin exists to *auto-negotiate* role from cable wiring. A
kernel built with `dr_mode = "host"` forces the role and never consults ID —
the standard arrangement on sunxi boards that wire a micro-B connector with
no ID routing. An unpopulated R4 blocks *automatic* OTG role-switching, not
host mode as such.

Confidence: **ASSUMED (strong)** — it follows from how `dr_mode` is handled in
`musb_core` / `phy-sun4i-usb`, but I did not verify it on this board and did
not read the 4.14 driver source this session. Not a MEASURED claim.

### 1.3 The EHCI0/OHCI0 wrinkle — now interesting rather than dismissible

Both host controllers are **already enabled in the DTS and already built into
the kernel**: `sun8i-v3s-funkey.dts:86-88` (`&ehci0 { status = "okay"; }`) and
`:113-115` (`&ohci0`), with `CONFIG_USB_EHCI_HCD=y`, `CONFIG_USB_OHCI_HCD=y`
and both `_PLATFORM` variants (`linux.config:110-113`).

Against that: `arch/arm/boot/dts/sun8i-v3s.dtsi` declares exactly **one** USB
PHY (`usbphy: phy@01c19400`, `reg-names = "phy_ctrl", "pmu0"` — a single PMU,
port 0), and `ehci0`/`ohci0` (`:308-325`) carry **no `phys` phandle at all**.
On the V3s these are the host side of the *same* USB0 port MUSB owns; there
is no second connector on the PCB.

Two readings, and source cannot separate them:
- **(a) Boilerplate** inherited from the sunxi board-DTS template, registering
  root hubs that can never enumerate. Most likely.
- **(b) Live host controllers** already sitting on USB0, starved only because
  MUSB holds the PHY in peripheral mode.

If (b), the fork is smaller than §1.4 suggests. **§7 rung 1 distinguishes them
in one command**, which is why that command is worth running before anyone
forms an opinion — mine included.

### 1.4 What the fork would actually take

All three edits are in trees the owner already forks and builds:

1. **DTS** — `sun8i-v3s-funkey.dts:251-254`, `dr_mode = "peripheral"` →
   `"host"` (or `"otg"`). One line.
2. **Kernel config** — `linux.config:115`, `CONFIG_USB_MUSB_GADGET=y` →
   `CONFIG_USB_MUSB_DUAL_ROLE=y`. One line. Add `CONFIG_HID` /
   `CONFIG_USB_HID` only if the generic-gamepad path is wanted; the libusb
   path in §3 does not need them.
3. **Buildroot packages** — `FunKey/configs/funkey_defconfig` has no libusb and
   no udev provider (grep: 0 matches). Add `BR2_PACKAGE_LIBUSB=y` and
   `BR2_PACKAGE_EUDEV=y`. Two lines; both stock buildroot packages.

Cost, per DrUm78's own README: a Docker build, **~1.5 h**, ~12 GB disk, plus
reflash. **Real, but ordinary.**

**One tradeoff to price in (item 2):** peripheral mode is how adb reaches this
device today, and adb is how every rig script, device check and M3/M4 gate
talks to it. Dual-role should preserve that, but "should" is doing work in
that sentence — verify adb survives before building anything on top of the
new image, and keep a known-good SD image to roll back to.

---

## 2. Q2 — Power: **not a blocker** (corrected, see C1)

The A33 ticket asks whether the device can "provide or bypass" the adapter's
500 mA (`fix_plan.md:4183`). Answer: **bypass, trivially, with no hub.**

### 2.1 The adapter is externally powerable by design (CITED)

Two plugs, one data:
- **Black — data + logic power.** The only *required* plug.
- **Grey — power only, for rumble.** Nintendo's support page says both should
  normally be inserted, but if you are short on ports only black is needed;
  the tradeoff is that rumble stops, because rumble is powered from grey.

Source: Nintendo Support, *How to Connect the GameCube Controller Adapter*
(`en-americas-support.nintendo.com/app/answers/detail/a_id/13287`), with
corroborating community threads (Smashboards, GBAtemp). Per-leg mA figures are
**UNKNOWN** — the Dolphin guide that would carry them could not be retrieved
(§6). The ticket's 500 mA is an unconfirmed aggregate that evidently includes
the rumble motors, which need not come from the host at all.

### 2.2 What survives, narrowly — and it is now the core question

The board cannot *source* VBUS. `usb0_vbus-supply = <&reg_vcc5v0>`
(`sun8i-v3s-funkey.dts:257`) resolves to
`arch/arm/boot/dts/sunxi-common-regulators.dtsi:105-110`:

```
reg_vcc5v0: vcc5v0 {
	compatible = "regulator-fixed";
	regulator-name = "vcc5v0";
	regulator-min-microvolt = <5000000>;
	regulator-max-microvolt = <5000000>;
};
```

`regulator-fixed` with **no `gpio` and no `vin-supply`** — a descriptor for the
incoming 5 V rail, not a switchable boost. No DRIVEVBUS / N_VBUSEN path
appears anywhere in the tree, so nothing pushes 5 V outward onto J2.

But **"cannot source VBUS" ≠ "the peripheral goes unpowered."** A host asserts
VBUS both to feed the device *and* to signal port power. C1 removes the first.
The second is the **central remaining unknown**: whether a sunxi host port
that never drives VBUS will still enable the port and enumerate a self-powered
device. Not answerable from the device tree. **This, not power budget, is what
ticket A33 now turns on.**

Also unresolved and worth care: VBUS on J2 is an **input** to the AXP209
charger, so feeding 5 V in from the adapter's power leg looks like plugging in
a charger. Probably benign, probably charges the console — but it back-feeds a
rail and is untested. Not something to try casually.

For scale only: `sun8i-v3s-funkey.dts:67-70` caps the battery at
`constant_charge_current_max_microamp = <400000>`.

---

## 3. Q3 — Driver shape (tractable)

**`ToadKing/wii-u-gc-adapter`** (WebFetch 2026-08-23, README). Linux
userspace; takes over the adapter and exposes each port as a separate virtual
input device. Requirements, verbatim:

> "*   libudev
> *   libusb(x) >= 1.0.16"

Needs root — it detaches the kernel driver and writes uinput — and the README
notes `modprobe uinput` if uinput is not autoloaded. License **MIT**. Its USB
transfer type and poll rate are not on the README and I did not read the
source (no vendoring, lane rule), so **§4's cost is unquantified**.

- **uinput: ALREADY PRESENT.** `linux.config:60` `CONFIG_INPUT_UINPUT=y`, with
  `CONFIG_INPUT_EVDEV=y` (`:56`). Corroborated in-tree: `port/tools/fk_input.c`
  creates its own uinput device, and every device check injects through it.
  The one requirement already satisfied, on both trees.
- **libusb / libudev: absent, trivially addable.** §1.4 item 3.

**`secretkeysio/gcadapterdriver`** (WebFetch 2026-08-23, README) — **macOS
only; does not apply.** Xcode kext plus DriverKit system extension, Apple
driver frameworks, no libusb; installs via `kextutil` / `systemextensionsctl`.
No Linux, embedded or handheld target anywhere in its README. Its purpose is
*overclocking* adapter poll rate for Slippi/Dolphin on macOS. Stated plainly
because it was owner-provided: not a portable driver — though its polling-rate
work may be worth reading if §4 turns out tight.

---

## 4. Q4 — Latency and CPU against 16.67 ms (UNKNOWN — the second real risk)

Not measurable without working hardware. Flagged as a genuine risk rather than
a footnote, because headroom is thin and this is the constraint most likely to
bite *after* the electrical question resolves favourably.

Measured sim-only cost on device: **p50 4.27-5.81 ms, p99 7.95-10.68 ms**
(`docs/research/device-perf.md`), leaving roughly 6 ms for render, present and
audio on one Cortex-A7. A libusb polling thread plus uinput round-trip plus
SDL event pump competes for that same core, and `wii-u-gc-adapter`'s poll rate
is unread (§3). Input latency also has a *quality* floor a frame budget does
not capture: a Melee port whose controller adds 2-3 frames of lag is worse
than the d-pad it replaced.

Must be measured, not assumed, before any GO is final.

---

## 5. Q5 — Licensing

- `ToadKing/wii-u-gc-adapter`: **MIT** (CITED).
- `secretkeysio/gcadapterdriver`: **MIT**, "Copyright 2021 SecretKeys LLC"
  (CITED).

Both compatible with a private, non-distributed build. Per `CLAUDE.md`
(Licensing/provenance), vendored third-party code needs its `NOTICES` entry
**before** it lands in-tree.

**Nothing was vendored by this spike, so no `NOTICES` change is required
now.** If A33 proceeds, the MIT notice must land in `NOTICES` in the same
commit as the first borrowed line, not after.

Separate, and worth surfacing early rather than discovering at rung 3: forking
DrUm78's OS means carrying a GPL-licensed kernel/buildroot fork. Private and
non-distributed, so no distribution obligation is triggered — but it is a new
maintenance surface (rebasing onto DrUm78's future releases, and re-applying
the §1.4 edits each time). That cost belongs in the owner's pricing of this
ticket, not in a verdict of mine.

---

## 6. Source that could not be retrieved

`dolphin-emu.org/docs/guides/how-use-official-gc-controller-adapter-wii-u/`
returned **HTTP 403** on WebFetch (2026-08-23); the wiki equivalent 404'd. The
owner called it "a really good resource".

Its content is **not reconstructed from memory here.** It also cost something
real: not having it is why §2's first draft was wrong (C1). The adapter-side
detail it carries — two plugs, one power-only — would have shown immediately
that 500 mA was not a host-side obligation. A normal browser retrieves the
page fine; only automated fetch is blocked. **Anyone taking this ticket
further should read it first**, particularly for per-leg current and any
polling-rate detail bearing on §4.

---

## 7. The experiment ladder

Ordered by cost. **Do not skip to rung 3.** Rung 1 is free and may sharpen or
end the ticket by itself.

**Rung 1 — free, needs only a reconnected device. Does a USB root hub exist?**
```
adb -s 12c00003237f5528 shell "sh -lc 'ls -d /sys/bus/usb/devices/usb* 2>&1; dmesg | grep -i -e musb -e ehci -e ohci -e usb0'"
```
Settles §1.3 (a)-vs-(b) on the *shipped* image. Live root hubs mean the fork is
smaller than §1.4 and confidence rises sharply; absent hubs is the expected
result and costs nothing.

**Rung 2 — needs an OTG cable, the adapter, and any USB charger for the grey
leg. No hub.** Still on the *stock* image: does anything enumerate? Expected
no, because MUSB holds the PHY in peripheral mode. A positive result would be
a large surprise and effectively a GO.

**Rung 3 — the real test, ~2 h of build plus reflash.** Fork DrUm78's OS with
the three §1.4 edits, flash, repeat rung 2. This answers the actual question:
whether USB0 drives a host bus with VBUS unsourced and ID floating (§2.2).
Keep dual-role so adb survives, verify adb first, and keep a known-good SD
image to roll back to.

**Rung 4 — only if 3 passes.** Cross-build `wii-u-gc-adapter` (MIT, §5),
confirm controllers appear via uinput, then measure §4 against the 16.67 ms
budget before calling it a GO.

Rungs 1-2 are cheap enough to run opportunistically next time the device is
connected for something else.

**None of these were run by this spike:** the device is disconnected and lane
rule 3 forbids device runs.

---

## 8. Stakes, and what NOT to close

The payoff is large, and worth restating because it is what justifies rung 3's
two hours. Per the ticket, a GO makes the "CONTROLLER" branch of the Controls
menu real (A24), justifies per-player bindings (A31), and — the real prize —
supplies a **true analog stick**, retiring the digital-d-pad compromise at
`port/gfx/ctl_style.h:14-23`: no walk, no partial DI angles, no angled
f-tilts, no C-stick, and tap jump forced off because "a digital d-pad at full
deflection tap-jumps on every upward DI".

**Driver: do NOT close A24, A31 or A32 on this spike's authority.** An earlier
draft of this document recommended exactly that, on the strength of a NO-GO
that no longer stands. Their "re-open if A33 lands" clauses stay open until at
least rung 3 has run:

- **A24** — keep the CONTROLLER branch open; do not scope it to built-in
  buttons yet.
- **A31** — the "retrofit if A33 lands" clause stays.
- **A32 / tapJumpOff** — its `fix_plan.md:4249` re-open note stays; P2+ may yet
  become human ports.

What *is* safe to act on today is unchanged: P1 is currently the only human
port, so shipping per-player UI for port 0 only remains right, and
`docs/research/b0xx-mapping.md` remains the answer for the built-in controls
however A33 resolves.

This lane changed no ticket rows.
