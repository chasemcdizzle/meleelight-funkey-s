# GameCube adapter over the FunKey-S micro-USB port — feasibility spike (A33)

Research ticket A33 (`fix_plan.md:4175`), lane R. Question: can the official
Wii U / GameCube controller adapter (or any USB controller) be supported on
the FunKey-S through its micro-USB port?

**VERDICT: NO-GO.** The FunKey-S micro-USB port cannot act as a USB host for
a bus-powered device, and this is settled in *hardware*, not in software.
The vendor's own hardware reference states the USB **ID pin is deliberately
unwired** and the board acts "only as an USB device"; the board's 5 V rail is
a fixed *input* regulator with no way to source VBUS onto the connector. Two
independent software locks (device-tree `dr_mode = "peripheral"` and a
gadget-only kernel) sit on top of that, and both live in the OS image, which
this project does not own and does not ship.

Q1 gates everything and Q1 is NO. §§2-5 below are recorded only to stop the
question being re-litigated, and because the power finding is the second
independent kill.

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

## 2. Q2 — Power: can the device supply or bypass the adapter's 500 mA? **NO.**

The 500 mA figure is CITED from the A33 ticket text (`fix_plan.md:4183`);
it could not be confirmed against the Dolphin page (§6).

The board cannot source VBUS at all. `usb0_vbus-supply = <&reg_vcc5v0>`
(`sun8i-v3s-funkey.dts:216`) points at a regulator defined in
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
board DTS only adds `regulator-always-on` (`:173-176`). There is no
DRIVEVBUS/N_VBUSEN path declared anywhere in the tree, so nothing can push
5 V outward onto J2.

For scale, the whole device's charge budget is below the adapter's ask:
`sun8i-v3s-funkey.dts:67-70` caps the battery at
`constant_charge_current_max_microamp = <400000>` — **400 mA** for the
console itself.

So even in a hypothetical patched-kernel world, the adapter would have to be
externally powered (a self-powered hub), which means a powered hub dangling
off a keychain console. That is not the feature the owner asked for.

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

Its content is **not reconstructed from memory here.** Nothing in this
document depends on it: the verdict rests on the FunKey vendor hardware
reference (§1.1) and the board's own device tree and kernel config
(§§1.2-1.3, §2), all of which were retrieved directly. If someone re-opens
this ticket, that page is worth reading for adapter-side detail, but it
cannot overturn a board-side finding.

---

## 7. What would have to change, and the one cheap experiment

Nothing in §1.1 or §2 is reachable from software. To overturn the verdict all
of the following would have to hold:

1. A **custom kernel build** with `CONFIG_USB_MUSB_DUAL_ROLE` (or MUSB
   dropped so EHCI0 can own the PHY), plus a DTS change off
   `dr_mode = "peripheral"`, plus `CONFIG_HID`/`CONFIG_USB_HID` if the
   generic path is wanted. This means **rebuilding and reflashing FunKey-OS**
   — an OS image this project neither owns nor ships. We ship an OPK.
2. An **externally powered** adapter or hub, because the board cannot source
   VBUS (§2).
3. `libusb` + `libudev` cross-built into the OPK (§3).

That is a hardware-and-firmware modification project, not a port feature.

**The one cheap experiment,** if anyone wants the §1.4 ambiguity closed for
the record (≈10 minutes, needs the device reconnected plus a micro-B OTG
cable and a self-powered USB hub):

```
# 1. did the DT-enabled host controllers register root hubs at all?
adb -s 12c00003237f5528 shell "sh -lc 'ls -d /sys/bus/usb/devices/usb* 2>&1'"

# 2. with a SELF-POWERED hub + any USB device attached to J2:
adb -s 12c00003237f5528 shell "sh -lc 'ls /sys/bus/usb/devices/; dmesg | tail -40'"
```

Outcome 1 (expected): no `usb*` root hub, or a root hub that never
enumerates — confirms peripheral-only, ticket closed permanently.
Outcome 2 (surprise): a device enumerates — the ceiling moves from
"impossible" to "needs a powered hub, a libusb cross-build, and no kernel
rebuild". Even then it is a NO for the owner's stated use, which is plugging
the official adapter straight into the console.

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
