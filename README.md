# Xterra ↔ Titan CAN Bus Gateway (Teensy 4.1)

A single-bus CAN gateway/translator that lets a **2005 Nissan Xterra (N50, manual)**
chassis run with a **2014 Nissan Titan VK56DE ECM**. Built on a **Teensy 4.1**
with **FlexCAN_T4**.

This repo documents an in-progress reverse-engineering effort. Findings are
labeled **confirmed** (verified against first-party captures or the factory
service manual) or **provisional** (working hypothesis, not yet verified). It is
shared so other Nissan swap builders don't have to start from zero.

> Status: drivable. Chassis communication faults cleared, 2WD↔4HI working,
> ECM kept out of lost-TCM comms codes. Open items: tach/RPM scaling, coolant
> temp decode, 4LO engagement, tune-level throttle limit. See **Project Status** below.

---

## Architecture — single bus, no split required

The most important finding, and the biggest change from earlier versions of this
project:

- The **Titan VK56DE ECM broadcasts on the 0x1xx ID range.**
- The **Xterra VQ40 chassis modules** (combination meter, ABS, TCCU/transfer)
  listen on the **0x23x range.**
- These ranges **do not overlap.** The ECM and chassis can therefore share **one
  physical CAN bus.** The Teensy simply **adds the frames the chassis expects but
  the Titan ECM never sends.**

There is **no need to physically split the bus** into separate chassis/powertrain
segments (an earlier design assumption that has since been dropped). A single
transceiver tapping one 500 kbit/s bus is sufficient. The gateway is an
**injector/translator on one bus**, not a two-port bridge.

```
        ┌────────────────────────────────────────────┐
        │              single 500k CAN bus            │
        │  Titan ECM (0x1xx)   Xterra chassis (0x23x)  │
        │        │                     │              │
        │        └───────── + ─────────┘              │
        │                   │                         │
        │             Teensy 4.1 (adds 0x23x frames   │
        │             the chassis needs)              │
        └────────────────────────────────────────────┘
```

---

## Hardware

| Component        | Purpose      | Notes                                            |
| ---------------- | ------------ | ------------------------------------------------ |
| Teensy 4.1       | Core MCU     | Native CAN controllers; this project uses CAN1   |
| CAN transceiver  | Bus I/O      | 3.3 V logic (e.g. SN65HVD230); 500 kbit/s        |
| Clean 12V→5V/3V3 | Power        | Reverse-polarity + TVS protection recommended    |

Bus speed is **500 kbit/s**. Termination follows normal CAN practice: the bus
should see ~60 Ω overall (two 120 Ω terminators). When tapping in parallel with
the existing OEM bus that is already terminated at the ECM and a chassis module,
the Teensy's transceiver should **not** add another 120 Ω terminator.

---

## Firmware

All sketches target **Teensy 4.1 / FlexCAN_T4**, CAN1, 500 kbit/s.

| Path                                          | Function                                                                 |
| --------------------------------------------- | ------------------------------------------------------------------------ |
| `firmware/gateway/gateway.ino`                | **Working gateway.** Presence frames + 0x23D engine data (advancing counter) + 0x251 neutral TCM spoof. Clears chassis comms faults, enables 2WD↔4HI. |
| `firmware/sniffer_freetext/sniffer_freetext.ino` | **Listen-only logger** with free-text markers (type any text + Enter to stamp the log). Used for capture sessions. Transmits nothing. |
| `firmware/tcm_spoof_test/tcm_spoof_test.ino`  | Staged 0x251 TCM gear-status spoof tester (modes A/B/C, gear toggle, rate). |
| `firmware/tach_target_test/tach_target_test.ino` | RPM-injection tester for finding the cluster/TCCU engine-speed frame/scale. |

See **`docs/SIGNALS.md`** for the CAN ID map and **`docs/GATEWAY_DESIGN.md`** for
how the gateway is structured.

---

## ⚠️ Safety note on the TCM spoof

The `0x251` neutral gear-status spoof in the gateway exists to clear the Titan
ECM's *lost-communication-with-TCM* code. It is **only appropriate because this
is a manual swap with no automatic transmission present.** It defeats a check
intended to coordinate with an A/T. **Do not use the TCM spoof on a vehicle that
still has an automatic transmission.**

The spoof clears the comms code but **does not** restore full throttle. The
remaining throttle/torque limit is **tune-level** (addressed via UpRev/Osiris),
not a CAN problem.

---

## Project status

**Confirmed (verified):**
- Single-bus architecture viable — Titan 0x1xx vs Xterra 0x23x ranges don't overlap.
- Titan engine RPM on `0x180` bytes 0–1, big-endian, 0.125 RPM/LSB (rev-sweep confirmed); mirrored on `0x1F9` bytes 2–3.
- Chassis comms faults (U1000) clear when the expected 0x23x presence frames are injected.
- 2WD↔4HI shifting works once 0x23D is provided with an advancing counter.
- `0x251` neutral replay clears the ECM lost-TCM comms code (manual swap).
- 4LO permit logic (per factory TF section): requires vehicle stopped, engine running, neutral + clutch + brake; vehicle speed reaches the TCCU via CAN from ABS; engine speed via CAN from ECM. Neutral switch circuit and wait-detection switch verified good on the build.

**Open / in progress:**
- **Tach/RPM scaling:** the exact 0x23x frame/byte/scale the cluster and TCCU read for engine speed is not yet confirmed. The TCCU currently reads a frozen/implausible engine speed from injected data. Pending a stock-VQ40 (Frontier) rev-sweep capture.
- **Coolant temp:** byte/scale for the cluster temp gauge not yet decoded. Pending Frontier cold-start warm-up capture.
- **4LO engagement:** does not engage yet (motor not commanded). Engine-speed signal to the TCCU is the leading suspect among remaining inputs; 4LO switch circuit on the rewired connector still to be verified.
- **Throttle/torque limit:** tune-level, pending UpRev/Osiris manual-conversion reflash.

**Dead ends / corrections (so others don't repeat them):**
- Dual-bus physical split — unnecessary; single bus works.
- `0x23E` is **throttle/torque, not RPM** (earlier assumption was wrong; it came from a discarded third-party log).
- TCM CAN emulation does not fix the throttle limit — that's tune-level.

---

## Repository layout

```
Xterra-Titan-Canbus-Gateway/
├─ README.md
├─ LICENSE
├─ docs/
│  ├─ SIGNALS.md          ← CAN ID map: confirmed vs provisional
│  └─ GATEWAY_DESIGN.md   ← architecture + how the gateway is built
├─ firmware/
│  ├─ gateway/            ← working gateway sketch
│  ├─ sniffer_freetext/   ← listen-only logger w/ free-text markers
│  ├─ tcm_spoof_test/     ← 0x251 TCM spoof tester
│  └─ tach_target_test/   ← RPM-injection tester
├─ data/
│  ├─ id_map.csv          ← machine-readable signal map
│  └─ logs/               ← capture logs (add your own; large files)
└─ scripts/
   └─ convert_savvycan.py ← text log → CSV for analysis
```

---

## Tools

- **SavvyCAN** for logging/replay; **Consult II/III** and a Topdon scan tool for module data.
- `scripts/convert_savvycan.py` converts text logs to CSV:
  ```
  python3 scripts/convert_savvycan.py data/logs/rev_sweep.txt data/logs/rev_sweep.csv
  ```

---

## License

MIT — see [LICENSE](LICENSE).

## Contributing

Doing a similar Nissan CAN swap? Forks and PRs welcome. The goal is to make
Titan-into-Xterra (and related F-Alpha/VQ40/VK56 platform) swaps easier to
document and reproduce.
