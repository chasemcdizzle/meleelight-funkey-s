# Where the FunKey-S host-controller USB data pins actually go — A33 follow-up

Follow-up to [`gc-adapter.md`](./gc-adapter.md). One question: **are the
"USB1" host-controller D+/D− pins routed to anything physically reachable
on the FunKey-S board?**

**VERDICT: ROUTED — and the ticket's premise needs correcting first.**

There is no second USB port to route. `/sys/bus/usb/devices/usb1` is a
Linux **bus number**, not a port number. The EHCI at `1c1a000` and the OHCI
at `1c1a400` are — in Allwinner's own memory map — **`USB OTG_EHCI0/OHCI0`**:
the host-side controllers of the one and only USB port the V3s has. Their
D+/D− are V3s pins 111/110, and those pins are wired straight to the
micro-USB connector **J2** (plus two bare test pads).

So the pins are not merely reachable — **they are the connector `adb` is
already plugged into.** Nothing needs soldering, no footprint is
unpopulated, no castellation is involved. What stands between those
controllers and a GameCube adapter is **one bit in one PHY register**,
`OTGCTL_ROUTE_MUSB`, which the shipped kernel sets to MUSB because the
board device tree says `dr_mode = "peripheral"`.

The owner was right that this is answerable from published sources. It was,
entirely: Allwinner's V3s datasheet and FunKey's own KiCad netlist are both
public, and the netlist is a machine-readable list of every pin and the net
it lands on. No multimeter, no teardown photo, no guessing.

- Board: FunKey-S **Rev E** (the repo's current master; assumed to be the
  owner's unit — see §7).
- Kernel: DrUm78/linux @ `v1.0.3-funkey-s` (4.14.14), the owner's baseline.
- **No device runs were performed by this lane** — the driver owns the
  device. Device facts below are the ones supplied with the ticket, labelled
  `MEASURED (given)`.

---

## 0. Three corrections to `gc-adapter.md`

Recorded rather than silently edited, per that document's own convention.

**D1 — §1.3's reading (a) is wrong, and it was the "most likely" one.**
That section offered two readings of the enabled EHCI0/OHCI0 nodes:
"(a) **Boilerplate** inherited from the sunxi board-DTS template,
registering root hubs that can never enumerate. Most likely." It is
**(b)**: live host controllers on the real port, starved because MUSB holds
the PHY. §5 below shows exactly why they probe, and §2 shows their pins are
the connector's pins. *Lesson: "boilerplate" was a story about the DTS. The
netlist and the memory map are about the silicon, and both were one fetch
away.*

**D2 — §1.4's `dr_mode = "host"` **(or `"otg"`)** — the parenthetical is
wrong.** With no `id-det-gpio` in this board's DTS, `dr_mode = "otg"` falls
through to the peripheral fallback and the PHY is routed to MUSB, exactly as
today. Only the literal string `"host"` yields `id_det = 0` and the reroute.
See the quoted `sun4i_usb_phy0_get_id_det()` table in §3.1. `CITED`

**D3 — §2.2's `usb_phy_generic` inference is not about the host
controllers.** The kernel line `usb_phy_generic ...: supply vcc not found,
using dummy regulator` comes from the **MUSB sunxi glue**
(`drivers/usb/musb/sunxi.c:779`, `usb_phy_generic_register()`), not from
EHCI or OHCI. It says nothing about host-side PHY state. `CITED`

Also worth flagging, though not load-bearing: §1.2 reported R4 unmounted on
the strength of the vendor doc page. The **Rev E netlist and interactive BOM
both carry R4 = 47 kΩ with no DNP field** — i.e. fitted, pulling `/USB_ID`
to GND. The doc's "should probably not be mounted" and the shipped BOM
disagree. It changes nothing here, because this kernel never reads the ID
pin (§3.1). `CITED` / conflict unresolved.

---

## 1. Q1 — Does the V3s bond out a second USB D+/D− pair? **No. There is no second pair.**

Primary source: **`Allwinner_V3s_Datasheet_V1.0.pdf`**, carried in FunKey's
own hardware repository at
`FunKey-Project/FunKey-S-Hardware/Datasheets/Allwinner_V3s_Datasheet_V1.0.pdf`
(downloaded and text-extracted 2026-08-24).

| Fact | Source | Label |
|---|---|---|
| "**One** USB 2.0 OTG controller with integrated PHY" | datasheet, USB feature list | `CITED` |
| "Complies with Enhanced Host Controller Interface (EHCI) Specification, Version 1.0, and the Open Host Controller Interface (OHCI) Specification, Version 1.0a **for host mode**" | same feature list, next bullet | `CITED` |
| Pin-description table, module **USB**: exactly three pins — `VCC-USB`, **`USB-DP0`**, **`USB-DM0`** | datasheet pin description | `CITED` |
| Package pin list: **110 = `USB-DM`**, **111 = `USB-DP`**, 109 = `VCC-USB` | datasheet pin list | `CITED` |
| Zero occurrences of `USB-DP1`, `USB-DM1`, `DP1` or `DM1` anywhere in the datasheet text | grep over the extracted text, 0 matches | `CITED` |
| Package is **LQFP128** | FunKey design log; netlist footprint `FunKey:ELQFP-128` | `CITED` |

Corroborated independently by FunKey's own V3S schematic symbol
(`FunKey.lib`, part `V3S`): of its pin definitions, exactly three match
`USB` — `VCC-USB` (109), `USB-DM` (110), `USB-DP` (111). `CITED`

**The `0` suffix in `USB-DP0` is a port index with no sibling.** Allwinner
number the block from zero across the family; on H3 there are USB0…USB3, on
V3s there is only USB0.

### 1.1 So what are `usb1` and `usb2` in sysfs?

Linux **bus numbers**, assigned in registration order. The addresses in the
`MEASURED (given)` readings settle it against the datasheet's memory map:

| sysfs | address | Datasheet memory-map entry |
|---|---|---|
| — (MUSB, `b_peripheral`) | `1c19000` | `USB OTG_Device` — `0x01C1 9000---0x01C1 9FFF` |
| `usb1` (EHCI, 480 Mb/s) | `1c1a000` | **`USB OTG_EHCI0/OHCI0`** — `0x01C1 A000---0x01C1 AFFF` |
| `usb2` (OHCI, 12 Mb/s) | `1c1a400` | same 4 K window as above |

`MEASURED (given)` for the left two columns, `CITED` for the right.

Allwinner named the block **`USB OTG_EHCI0/OHCI0`** — the host controllers
*of the OTG port*. There is no separate host block, and the SoC device tree
agrees: `sun8i-v3s.dtsi` declares one `usbphy` (`phy@01c19400`, with a single
`pmu0` region), one `ehci0` (`usb@01c1a000`), one `ohci0` (`usb@01c1a400`),
and **no `ehci1`/`ohci1` node at all**. `CITED`

**Answer: the V3s does not bond out a second USB data pair, because it does
not have one. The question as posed has no yes/no answer — but the useful
answer is better than yes.**

---

## 2. Q2 — Does the schematic route the host controller's pins anywhere? **Yes: to the micro-USB connector, and to two test pads.**

Primary source: **`FunKey.net`**, the KiCad netlist exported from
`FunKey.sch`, Rev E, dated 27 Nov 2020, in
`FunKey-Project/FunKey-S-Hardware` (downloaded 2026-08-24). A netlist is an
exhaustive list of nets and the component pins on each — the strongest form
of the evidence this ticket asked for.

There are exactly **three** USB signal nets in the entire board. Verbatim:

```
(net (code 139) (name /USB_P)
  (node (ref J2) (pin 3))
  (node (ref U3) (pin 111))
  (node (ref TP27) (pin 1))
  (node (ref D15) (pin 2)))
(net (code 140) (name /USB_N)
  (node (ref J2) (pin 2))
  (node (ref D15) (pin 3))
  (node (ref U3) (pin 110))
  (node (ref TP26) (pin 1)))
(net (code 143) (name /USB_ID)
  (node (ref J2) (pin 4))
  (node (ref R4) (pin 1)))
```

Plus `+VUSB` (net 116), which reaches `U5` pin 31 — the AXP209 — confirming
`gc-adapter.md` §2.2's finding that VBUS on J2 is a charger **input**.

Reference designators, from the netlist's component section: `CITED`

| Ref | Part | Meaning |
|---|---|---|
| `U3` | `V3S`, footprint `FunKey:ELQFP-128` | the SoC |
| `J2` | `U02-BFD3111B0-009` | the micro-USB connector |
| `D15` | `PRTR5V0U2X,215` | NXP TVS / ESD clamp array on D+/D− |
| `TP26` / `TP27` | footprint `FunKey:TEST_PAD`, values `D-` / `D+` | bare test pads |
| `R4` | 47 kΩ 0402, pin 2 → `GND` | ID pull-down |

**So V3s pin 111 (`USB-DP`) → `J2` pin 3, and pin 110 (`USB-DM`) → `J2`
pin 2**, through the ESD array and the CLC filter the vendor doc describes
(`C7`, `C8`, `C10`, ferrite `L2`). `CITED`

Because the EHCI0/OHCI0 block shares the single PHY on those same two pins
(§1, §3), **the host controllers' data pins are the micro-USB connector's
data pins.** The routing question is answered by the connector already
soldered to the board.

---

## 3. Q3 — What would reaching them require? **Physically, nothing. A USB OTG cable.**

No soldering. No unpopulated connector. `TP26`/`TP27` exist as bare pads on
D−/D+ but are redundant — J2 carries the same nets, and it is the port `adb`
uses today.

The entire cost is software, and it is one register bit reached by one DTS
string.

### 3.1 The routing mechanism (the actual gate)

`drivers/phy/allwinner/phy-sun4i-usb.c` @ `v1.0.3-funkey-s`: `CITED`

```
{ .compatible = "allwinner,sun8i-v3s-usb-phy", .data = &sun8i_v3s_cfg }
```
and in `sun8i_v3s_cfg`:
```
.num_phys = 1,
.phy0_dual_route = true,
```

`phy0_dual_route` means PHY0's signals can be steered to **either** host-side
consumer. `sun4i_usb_phy0_reroute()`:

```
/* Host mode. Route phy0 to EHCI/OHCI */
regval &= ~OTGCTL_ROUTE_MUSB;
...
/* Peripheral mode. Route phy0 to MUSB */
regval |= OTGCTL_ROUTE_MUSB;
```
with `#define OTGCTL_ROUTE_MUSB BIT(0)` in `#define REG_PHY_OTGCTL 0x20`.

The choice is driven entirely by `id_det`, and `sun4i_usb_phy0_get_id_det()`
derives that from `dr_mode` when no GPIO is present: `CITED`

| `dr_mode` | `id_det` with no `id-det-gpio` | PHY0 routed to |
|---|---|---|
| `"host"` | 0 | **EHCI/OHCI** |
| `"peripheral"` | 1 | MUSB |
| `"otg"` | 1 (explicit `/* Fallback to peripheral mode */`) | MUSB |

The board DTS (`sun8i-v3s-funkey.dts`) sets `dr_mode = "peripheral"` on
`&usb_otg`, and declares **no** `usb0_id_det` or `usb0_vbus_det` property.
`CITED` — hence D2 above: `"otg"` is not a substitute for `"host"` here.

### 3.2 One structural catch worth knowing before anyone edits a DTS

`ehci0` and `ohci0` carry **no `phys` phandle** in `sun8i-v3s.dtsi`, and
`ehci-platform.c` counts its PHYs from exactly that property
(`priv->num_phys = of_count_phandle_with_args(dev->dev.of_node, "phys", ...)`).
`CITED`

Consequence: **EHCI0 cannot initialise the PHY itself.** The only consumer
holding `phys = <&usbphy 0>` is `&usb_otg` (MUSB), and `phy_init()` is what
schedules the detect work that performs the reroute. So a fork cannot simply
delete MUSB — MUSB must still probe successfully, in host or dual-role, for
the PHY to be initialised and rerouted at all.

That is consistent with `gc-adapter.md` §1.4's edits 1+2 being *both*
required, and it is a reason not to try edit 1 alone. `ASSUMED (strong)` —
it follows directly from the two cited mechanisms, but was not run.

---

## 4. Q4 — Published FunKey statements on USB host capability

Two, and they agree with everything above. `CITED`

FunKey design log, *Schematics: CPU*
(`hackaday.io/project/164934-funkey-project-all-your-games-on-your-keychain/log/162026-schematics-cpu`,
fetched 2026-08-24) — the blocks the design uses:

> "the AUDIO, **USB (as device only)**, SDC0 (for SD Card), RTC, DRAM, SPI
> (for the LCD screen), PWM0 (for backlight), TWI0"

Vendor hardware reference, *USB*
(`doc.funkey-project.com/developer_guide/hardware_reference/usb/`,
fetched 2026-08-24):

> "although **the V3s is able to work as either an USB host or USB device
> using the USB OTG protocol**, we don't need the ID pin to determine by the
> cable wiring which role we must take."

Read together: the project states plainly that host mode exists in the
silicon and that they chose not to use it. **Neither source claims the
hardware cannot do host.** The design page's "as device only" is a statement
of intent about firmware and the ID pin, not a claim about routing — and the
netlist shows the routing is identical either way, because there is only one
port to route.

No FunKey source found states that USB host works, or that anyone has tried
it. That remains untested territory, and §6 keeps the open part open.

---

## 5. Q5 — Why do EHCI/OHCI probe and register root hubs?

The ticket's hint — "a host controller with no downstream port still
registers a root hub" — is a real phenomenon, but **it is not what is
happening here.** The chain is simpler and more encouraging:

1. The board DTS enables both: `&ehci0 { status = "okay"; };` and
   `&ohci0 { status = "okay"; };`. `CITED`
2. `ehci-platform` / `ohci-platform` bind, enable the clocks and resets the
   dtsi gives them, and call `usb_add_hcd()`, which registers a root hub.
   They manage **zero** PHYs, because `ehci0`/`ohci0` have no `phys`
   property (§3.2) — so nothing in that path can fail for want of a PHY.
   `CITED`
3. Each root hub reports **`maxchild 1`** — one downstream port
   (`MEASURED (given)`). That port is real: it is the EHCI0 port on the
   shared USB0 pins, i.e. J2.
4. Meanwhile `phy-sun4i-usb` owns PHY0 and, seeing `dr_mode = "peripheral"`
   → `id_det = 1`, sets `OTGCTL_ROUTE_MUSB` — steering PHY0 to MUSB, which
   reports `b_peripheral` and runs adbd. `CITED` + `MEASURED (given)`

**So the root hubs are not phantoms and the port is not absent. The port is
the micro-USB connector, and its analog signal path is switched to the
gadget controller.** Plugging a device into J2 today cannot enumerate on bus
1 or 2 — not because the controllers are dead, but because the wire is
pointed elsewhere.

And the `usb_phy_generic ... supply vcc not found` line is MUSB's own glue
registering a NOP transceiver (`drivers/usb/musb/sunxi.c:779`), unrelated to
the host controllers (D3). `CITED`

---

## 6. What this settles, and what it does not

**Settled — the electrical routing question is closed.** `gc-adapter.md`'s
rung-1 experiment has effectively been run (the ticket's measurements) and
its result now has a source-backed explanation: live host controllers, real
port, real pins, connector already fitted. Nobody needs a multimeter, and
nobody needs to solder.

**Not settled — VBUS.** `gc-adapter.md` §2.2's central unknown survives this
document untouched: the board cannot **source** VBUS (`reg_vcc5v0` is a
fixed regulator describing the incoming rail, with no GPIO and no
`vin-supply`, and `+VUSB` feeds the AXP209 as an input). Whether a sunxi
host port that never asserts VBUS will still enable its port and enumerate a
self-powered device is a driver-behaviour question this lane did not touch.
That remains the question a fork actually tests.

**Not settled — latency and CPU** (`gc-adapter.md` §4), unchanged.

Net effect on the ticket: the *hardware* half of A33's risk is gone. What
remains is a kernel fork whose cost `gc-adapter.md` §1.4 already prices at
~1.5 h of Docker build plus a reflash, with the adb-survival caveat and a
known-good SD image to roll back to. **This document does not authorise that
build and closes no ticket rows** — it removes one reason not to.

---

## 7. Method, and what was not verified

**Sources, all fetched or downloaded 2026-08-24:**

| Artifact | URL |
|---|---|
| V3s datasheet V1.0 (PDF, 5.0 MB, text-extracted) | `raw.githubusercontent.com/FunKey-Project/FunKey-S-Hardware/master/Datasheets/Allwinner_V3s_Datasheet_V1.0.pdf` |
| KiCad netlist, Rev E | `raw.githubusercontent.com/FunKey-Project/FunKey-S-Hardware/master/FunKey.net` |
| KiCad schematic + symbol library | `.../master/FunKey.sch`, `.../master/FunKey.lib` |
| Interactive BOM | `.../master/BOM/ibom.html` |
| SoC device tree | `raw.githubusercontent.com/DrUm78/linux/v1.0.3-funkey-s/arch/arm/boot/dts/sun8i-v3s.dtsi` |
| Board device tree | `.../v1.0.3-funkey-s/arch/arm/boot/dts/sun8i-v3s-funkey.dts` |
| USB PHY driver | `.../v1.0.3-funkey-s/drivers/phy/allwinner/phy-sun4i-usb.c` |
| MUSB sunxi glue | `.../v1.0.3-funkey-s/drivers/usb/musb/sunxi.c` |
| EHCI platform driver | `.../v1.0.3-funkey-s/drivers/usb/host/ehci-platform.c` |
| Vendor hardware reference, USB | `doc.funkey-project.com/developer_guide/hardware_reference/usb/` |
| Design log, Schematics: CPU | `hackaday.io/project/164934-.../log/162026-schematics-cpu` |

The hardware repository is **CC BY-NC-SA 4.0**; nothing from it is vendored
here, only cited.

**Fetch failure:** `linux-sunxi.org/V3s` returned **HTTP 403**. Its content
is **not reconstructed from memory.** It was wanted only as corroboration
for the single-USB-port fact, which the Allwinner datasheet, the FunKey
symbol library and the SoC dtsi already establish three independent ways.
Nothing in this document depends on it.

**Not verified:**
- **Board revision.** All schematic claims are Rev E, the repo's current
  master. The owner's unit is `ASSUMED` to be Rev E; earlier revisions were
  not checked. USB routing on a V3s cannot differ in kind — there is only
  one pin pair — but designator numbers could.
- **The PCB layout** (`FunKey.kicad_pcb`, `PDF/FunKey PCB.pdf`) was not
  opened; net connectivity was taken from the netlist, which is exported
  from the schematic. Which side `TP26`/`TP27` sit on is unknown, and does
  not matter — J2 carries the same nets.
- **R4's true fitted state on shipped units** (§0), doc vs BOM conflict.
- **No device commands were run**, and no build was attempted.
