# GameCube adapter over the FunKey-S micro-USB port — feasibility spike (A33)

Research ticket A33 (`fix_plan.md:4175`), lane R. Question: can the official
Wii U / GameCube controller adapter (or any USB controller) be supported on
the FunKey-S through its micro-USB port?

**VERDICT: NO-GO.** The FunKey-S micro-USB port cannot act as a USB host.
The vendor's own hardware reference states the USB **ID pin is deliberately
unwired** and the board acts "only as an USB device", and the shipped kernel
contains **no MUSB host code at all** — it is not disabled, it is not
compiled. Undoing that means rebuilding and reflashing FunKey-OS, an image
this project neither owns nor ships. We ship an OPK.

Q1 gates everything and Q1 is NO. §§2-5 below are recorded only to stop the
question being re-litigated.

> **CORRECTION, same session — owner challenge.** The first draft of this
> document called power "a second independent kill" and told the reader they
> would need a self-powered USB hub. **Both claims are retracted; see §2.**
> The owner pointed out that the official adapter has *two* USB plugs, the
> grey one being power-only for rumble — so 500 mA is not the binding number
> and no hub is required. The verdict does not change, because **Q1 alone
> carries it**, but power is now a weak supporting point rather than a kill.
> Recorded in place rather than silently edited, so the reasoning stays
> auditable. Lesson for the next spike: I reached for a second kill before
> the first one needed help, and got the physics of the peripheral wrong.

- Device: FunKey-S, FunKey-OS 2.3.0, kernel **4.14.14-funkey**, Allwinner V3s
  (single Cortex-A7 @ 1.2 GHz), ADB id `12c00003237f5528`.
- **No device runs were performed** — the device is disconnected for this
  lane (lane rule 3). Every claim below is from primary source text: the
  FunKey-Project OS/kernel repositories, the FunKey vendor documentation, and
  the two owner-provided GitHub projects. Sources are pinned by path and tag.
- One owner-provided source could not be retrieved: see §6.

---

## 1. Q1 — Does the micro-USB port do USB host? **NO.**

### 1.1 Hardware: the ID pin is not wired (CITED — vendor primary source)

FunKey Project hardware reference, *USB*
(`doc.funkey-project.com/developer_guide/hardware_reference/usb/`, fetched
2026-08-23). The page states the port has exactly two functions — supplying
+5 V to power the device and charge the LiPo, and carrying data for firmware/
ROM transfer — and then, verbatim:

> "the V3s is able to work as either an USB host or USB device using the USB
> OTG protocol"

…but, because the board only ever operates as a device:

> "we don't need the ID pin to determine by the cable wiring which role we
> must take."

And on the schematic component itself:

> "The resistor **R4** on the USB ID connector pin should probably not be
> mounted" — "as we act only as an USB device, this pin should be left
> floating."

The connector is an edge-mounted Micro-USB, designator **J2**. Wired nets are
+5 V, GND, D+ and D− only.

This is the decisive fact. The SoC is dual-role capable; **the board is not
wired to use it.** An OTG cable signals host-role by grounding ID, and on
this board ID terminates in an unpopulated pull-up.

### 1.2 Software lock A: device tree pins the controller to peripheral (CITED)

`FunKey-Project/linux`, tag `linux-FunKey-1.0.2`,
`arch/arm/boot/dts/sun8i-v3s-funkey.dts:210-213`:

```
&usb_otg {
	dr_mode = "peripheral";
	status = "okay";
};
```

The board's `&usbphy` node (`:215-220`) supplies `usb0_vbus-supply`,
`phy-supply` and `vcc`, and declares **no ID-detect GPIO** — so the kernel
performs no role detection at all. Peripheral is not a default here, it is
an assertion.

This DTS is the one that ships: `FunKey/configs/funkey_defconfig` sets
`BR2_LINUX_KERNEL_INTREE_DTS_NAME="sun8i-v3s-funkey"`.

### 1.3 Software lock B: the kernel has no MUSB host code (CITED)

`FunKey-Project/FunKey-OS`, `FunKey/board/funkey/linux.config` — the exact
file the build consumes (`funkey_defconfig`:
`BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE=".../board/funkey/linux.config"`),
lines 107-118:

```
# CONFIG_HID is not set
# CONFIG_USB_HID is not set
CONFIG_USB=y
CONFIG_USB_EHCI_HCD=y
CONFIG_USB_EHCI_HCD_PLATFORM=y
CONFIG_USB_OHCI_HCD=y
CONFIG_USB_OHCI_HCD_PLATFORM=y
CONFIG_USB_MUSB_HDRC=y
CONFIG_USB_MUSB_GADGET=y
CONFIG_USB_MUSB_SUNXI=y
CONFIG_NOP_USB_XCEIV=y
CONFIG_USB_GADGET=y
```

`CONFIG_USB_MUSB_GADGET=y` with no `CONFIG_USB_MUSB_HOST` and no
`CONFIG_USB_MUSB_DUAL_ROLE`. In Linux 4.14 these three are a **mutually
exclusive Kconfig choice** (`drivers/usb/musb/Kconfig`), so the host-side
MUSB code is not merely disabled at runtime — it is not compiled.

`# CONFIG_HID is not set` also means there is **no HID stack whatsoever**.
(This does not block the libusb approach of §3, which detaches from kernel
drivers anyway, but it does rule out "any USB gamepad just works".)

The gadget list below those lines (`USB_FUNCTIONFS`, `USB_MASS_STORAGE`,
`USB_ETH`, `USB_G_SERIAL`) is the port's actual job, and matches observed
behaviour: the `adb` marker file at SD root starts adbd over USB
(`CLAUDE.md`, Device access), and adbd is a FunctionFS gadget.

### 1.4 The one apparent loophole, closed: EHCI0/OHCI0 are enabled

Honest complication, because it looks like a way out. The board DTS **does**
enable both host controllers — `sun8i-v3s-funkey.dts:86-88` `&ehci0 { status
= "okay"; }` and `:113-115` `&ohci0 { status = "okay"; }` — and both drivers
are built in (§1.3). Taken alone that reads as "host support is present".

It does not survive contact with the SoC device tree.
`arch/arm/boot/dts/sun8i-v3s.dtsi` declares exactly **one** USB PHY
(`usbphy: phy@01c19400`, `reg-names = "phy_ctrl", "pmu0"` — a single PMU,
port 0 only), and `ehci0`/`ohci0` (`:308-325`) carry **no `phys` phandle at
all**. On the V3s, EHCI0/OHCI0 at `0x01c1a000`/`0x01c1a400` are the *host
side of the same USB0 port* that MUSB owns — there is no second physical
port for them to serve, and the FunKey PCB exposes no second connector.
These two `status = "okay"` lines are almost certainly inherited boilerplate
from the sunxi board-DTS template.

Whether those controllers register a root hub at boot is **UNKNOWN** and
would take one command to answer (§7). It does not change the verdict,
because §1.1 and §2 are hardware-final for the owner's actual use case.

---

## 2. Q2 — Power: **NOT the blocker.** (Corrected — the ticket's framing was wrong)

The A33 ticket frames this as "the official adapter expects 500 mA and has
its own supply leg — can this device provide or bypass it?"
(`fix_plan.md:4183`). **The answer is "bypass, trivially, with no hub" — and
that makes the whole 500 mA question moot rather than fatal.**

### 2.1 The adapter is already externally powerable by design (CITED)

The official adapter terminates in **two** USB plugs, and only one carries
data:

- **Black plug — data + logic power.** This is the only plug that is
  *required*. Nintendo's own support page says both should normally be
  inserted, but if you are short on ports only the black plug is needed.
- **Grey plug — power only, for rumble.** Nintendo states the tradeoff of
  omitting it is simply that rumble stops working, because rumble is powered
  from the grey leg. Community reports corroborate that unplugging grey was
  for a time the standard way to disable rumble.

Sources: Nintendo Support, *How to Connect the GameCube Controller Adapter*
(`en-americas-support.nintendo.com/app/answers/detail/a_id/13287`), plus
corroborating community threads (Smashboards, GBAtemp). The Dolphin guide
the owner recommended could not be retrieved (§6), so the mA breakdown per
leg is **UNKNOWN** — the 500 mA in the ticket is an aggregate figure that
was never confirmed against a primary source, and it evidently includes the
rumble motors, which live on the leg that does not have to come from the
host at all.

**Consequence: the earlier "you would need a self-powered hub" claim was
wrong.** The grey leg is a bare power input; any phone charger or battery
pack feeds it. There is no hub in this picture.

### 2.2 What is actually true, and much narrower

The board still cannot *source* VBUS on J2. `usb0_vbus-supply =
<&reg_vcc5v0>` (`sun8i-v3s-funkey.dts:216`) points at a regulator defined in
`arch/arm/boot/dts/sunxi-common-regulators.dtsi:105-110`:

```
reg_vcc5v0: vcc5v0 {
	compatible = "regulator-fixed";
	regulator-name = "vcc5v0";
	regulator-min-microvolt = <5000000>;
	regulator-max-microvolt = <5000000>;
};
```

A `regulator-fixed` with **no `gpio` and no `vin-supply`** — an always-on
descriptor for the incoming 5 V rail, not a switchable boost converter. The
board DTS only adds `regulator-always-on` (`:173-176`). No DRIVEVBUS /
N_VBUSEN path is declared anywhere in the tree, so nothing can push 5 V
outward onto J2.

But **"cannot source VBUS" is not the same as "the peripheral goes
unpowered"**, which is where the first draft went wrong. A USB host asserts
VBUS for two reasons: to feed a bus-powered device, and to signal port power
so the device knows to attach. The first reason evaporates here — the
adapter can take its logic power from an external source. The second is a
genuine **UNKNOWN**: whether a sunxi EHCI root port that never drives VBUS
will still enable the port and enumerate a self-powered device is not
answerable from the device tree, and I did not test it (no device, §7).

A further wrinkle, noted and *not* resolved: on this board VBUS on J2 is an
**input** to the AXP209 charger. Feeding 5 V in from the adapter's power leg
would be indistinguishable from plugging in a charger — probably benign, and
it would charge the console, but it is untested and involves back-feeding a
rail, so nobody should try it casually.

For scale only, not as an argument: `sun8i-v3s-funkey.dts:67-70` caps the
battery at `constant_charge_current_max_microamp = <400000>` — 400 mA for
the console itself.

**Net: power is a solvable engineering nuisance, not the blocker. Q1 is the
blocker.**

---

## 3. Q3 — Driver shape (MOOT, recorded)

**`ToadKing/wii-u-gc-adapter`** (WebFetch 2026-08-23, README). Linux
userspace, takes over the adapter and exposes each port as a separate virtual
input device. Requirements, quoted verbatim from the README:

> "*   libudev
> *   libusb(x) >= 1.0.16"

Needs root (it detaches the kernel driver and writes uinput), and the README
notes `modprobe uinput` if uinput is not autoloaded. License **MIT**. Its
USB transfer type and polling rate are not documented on the README page and
were not read from source (no vendoring — lane rule).

- **uinput: PRESENT.** `linux.config:60` `CONFIG_INPUT_UINPUT=y`, alongside
  `CONFIG_INPUT_EVDEV=y` (`:56`). Independently corroborated in-tree:
  `port/tools/fk_input.c` creates its own uinput keyboard device and every
  device check injects through it (`port/tools/fk_input.c:1-12`). This is the
  one requirement the FunKey-S already satisfies.
- **libusb / libudev: ABSENT.** Neither `FunKey/configs/funkey_defconfig` nor
  `SDK/configs/funkey_defconfig` contains any `libusb`, `libudev`, `udev` or
  `eudev` entry (grep, 0 matches). They would have to be cross-built against
  the SDK — routine work, but pointless without a host port.

**`secretkeysio/gcadapterdriver`** (WebFetch 2026-08-23, README). **This
project is macOS-only and does not apply here at all.** It ships an Xcode
kext and a DriverKit system extension, uses Apple driver frameworks (no
libusb), and its install steps are `kextutil` / `systemextensionsctl`. Its
README mentions no Linux, embedded, or handheld target. Its stated purpose is
*overclocking* the adapter's poll rate for Slippi/Dolphin on macOS. Worth
stating plainly since it was owner-provided: it is not a portable driver.

---

## 4. Q4 — Latency and CPU against the 16.67 ms budget (UNKNOWN, unmeasurable)

Not measurable without working hardware, and moot. For the record: the
binding constraint is real. Measured sim-only frame cost on device is
**p50 4.27-5.81 ms, p99 7.95-10.68 ms** (`docs/research/device-perf.md`),
leaving roughly 6 ms for render, present and audio on a single Cortex-A7.
A libusb polling thread plus uinput round-trip plus SDL event pump would
compete for that same core. Any future revisit must measure, not assume.

---

## 5. Q5 — Licensing

- `ToadKing/wii-u-gc-adapter`: **MIT** (CITED).
- `secretkeysio/gcadapterdriver`: **MIT**, "Copyright 2021 SecretKeys LLC"
  (CITED).

Both are compatible with a private, non-distributed build. Per the project
licensing rule (`CLAUDE.md`, Licensing/provenance), any vendored third-party
code needs its `NOTICES` entry **before** it lands in-tree.

**Nothing was vendored, copied or committed by this spike, so no `NOTICES`
change is required.** If A33 were ever revived, the MIT notice must be added
to `NOTICES` in the same commit that introduces the first line of borrowed
code, not after.

---

## 6. Source that could not be retrieved

`dolphin-emu.org/docs/guides/how-use-official-gc-controller-adapter-wii-u/`
returned **HTTP 403 Forbidden** on WebFetch (2026-08-23). The owner called it
"a really good resource" and it is the likely origin of the 500 mA figure and
of the adapter's two-plug (data + power) arrangement.

Its content is **not reconstructed from memory here.** The verdict does not
depend on it: that rests on the FunKey vendor hardware reference (§1.1) and
the board's own device tree and kernel config (§§1.2-1.3), all retrieved
directly.

**It did, however, cost something.** Not having it is why §2's first draft
went wrong: the 500 mA figure was taken from the ticket and treated as a
host-side obligation, when the adapter-side detail this page carries — two
plugs, one of them power-only — would have shown immediately that it is not.
The owner had read that page and I had not; that asymmetry is the whole
error. Anyone re-opening this ticket should read it first (a normal browser
retrieves it fine; only automated fetch is blocked).

---

## 7. What would have to change, and the one cheap experiment

To overturn the verdict all of the following would have to hold:

1. A **custom kernel build** with `CONFIG_USB_MUSB_DUAL_ROLE` (or MUSB
   dropped so EHCI0 can own the PHY), plus a DTS change off
   `dr_mode = "peripheral"`, plus `CONFIG_HID`/`CONFIG_USB_HID` if the
   generic path is wanted. This means **rebuilding and reflashing FunKey-OS**
   — an OS image this project neither owns nor ships. We ship an OPK.
   **This is the blocker.** It is not reachable from anything we build.
2. The adapter powered from its **own grey leg** (§2.1) — cheap and easy, no
   hub — plus confirmation that a never-powered root port still enumerates a
   self-powered device (§2.2, UNKNOWN).
3. `libusb` + `libudev` cross-built into the OPK (§3) — routine.

Only item 1 is hard, and it is hard in a way that is out of scope: it makes
this a firmware-modification project, not a port feature.

**The one cheap experiment,** if anyone wants §1.4 and §2.2 closed for the
record (≈10 minutes, needs the device reconnected, a micro-B OTG cable, and
the adapter's grey plug in any charger — **no hub**):

```
# 1. did the DT-enabled host controllers register root hubs at all?
adb -s 12c00003237f5528 shell "sh -lc 'ls -d /sys/bus/usb/devices/usb* 2>&1'"

# 2. adapter black plug -> J2 via OTG cable, grey plug -> any charger:
adb -s 12c00003237f5528 shell "sh -lc 'ls /sys/bus/usb/devices/; dmesg | tail -40'"
```

Outcome 1 (expected): no `usb*` root hub, or a root hub that never
enumerates — confirms peripheral-only, ticket closed permanently.
Outcome 2 (surprise): the adapter enumerates — then the shipped kernel is
doing more than its config suggests, §1.4 flips, and the ceiling moves from
"impossible" to "needs a libusb cross-build and a charger for the grey leg".
**That outcome would be worth a GO reconsideration**, which is the honest
consequence of the owner's correction: with the hub gone and the mA figure
moot, outcome 2 is no longer an absurd end state.

Note that step 1 costs nothing and needs no adapter, no cable and no
charger — it is one command against a reconnected device, and it alone
decides whether step 2 is worth setting up.

This experiment is **not** run by this spike: the device is disconnected and
lane rule 3 forbids device runs.

---

## 8. What the NO-GO costs, stated plainly

A GO would have been large. Per the ticket, it would have made the
"CONTROLLER" branch of the Controls menu real (A24), justified per-player
bindings (A31), and — the real prize — supplied a true analog stick, retiring
the whole digital-d-pad compromise documented at `port/gfx/ctl_style.h:14-23`:
no walk, no partial DI angles, no angled f-tilts, no C-stick, and tap jump
forced off because "a digital d-pad at full deflection tap-jumps on every
upward DI".

None of that is recoverable through this port. **The digital-d-pad
compromise is permanent on this hardware, and the analog mapping work
(`docs/research/b0xx-mapping.md`) is not a stopgap — it is the answer.**
Consequently:

- **A24 (Controls menu "CONTROLLER" branch):** no second physical controller
  will ever appear. Scope it to the built-in buttons.
- **A31 (per-player bindings):** the existing recommendation to ship the UI
  editing port 0 only stands, and the "retrofit if A33 lands" clause is now
  dead. Remove it.
- **A32 / tapJumpOff:** its "re-open only if A33 lands" note
  (`fix_plan.md:4249`) is likewise dead — P2+ will never be human ports on
  this device. P1 remains the only human port.

Driver: these three follow-ups are the actionable output of this spike and
are for the driver to apply; this lane changed no ticket rows.

**One honest caveat on all three.** After the §2 correction, the residual
uncertainty is no longer negligible — it is one free command (§7 step 1,
needs only a reconnected device). If that command shows a live USB root hub,
§1.4 flips and A33 deserves a second look before A24/A31/A32 are closed on
its authority. Cheap enough that the driver should just run it rather than
close three tickets on a NO-GO that has one untested assumption left in it.
