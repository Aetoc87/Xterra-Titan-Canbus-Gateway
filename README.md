# Xterra ↔ Titan CAN Bus Gateway (Teensy 4.1)

A single-bus CAN gateway/translator that lets a **2005 Nissan Xterra (N50, manual)**
chassis run with a **2014 Nissan Titan VK56DE ECM**. Built on a **Teensy 4.1**
with **FlexCAN_T4**.

This repo documents a working reverse-engineering effort. Findings are labeled
**confirmed** (verified against first-party captures, the factory service manual,
or live cross-checks) or **provisional**. Shared so other Nissan swap builders
don't start from zero.

> Status: **tach live and verified, 4HI + 4LO working, coolant gauge working,
> chassis comms faults cleared.** Remaining: live coolant translation (placeholder
> for now), SES light, and the tune-level throttle limit. See **Project Status**.

---

## Architecture — single bus, no split required

- The **Titan VK56DE ECM broadcasts on the 0x1xx ID range.**
- The **Xterra VQ40 chassis modules** (cluster, ABS, TCCU/transfer) listen on the
  **0x23x range.**
- The ranges **do not overlap**, so the ECM and chassis share **one physical CAN
  bus.** The Teensy reads the one signal they disagree on (engine RPM) and
  re-broadcasts it in the format the chassis expects, plus the presence frames
  the chassis needs.

No physical bus split, no second transceiver. The gateway is an
**injector/translator on one bus**. Validated in practice: with the ECM connected
and the gateway running, the cluster, ABS, and TCCU all read the same correct RPM.

---

## How the tach works (the core translation)

The single most important translation, confirmed end-to-end:

```
READ   Titan 0x180  bytes 0-1  BIG-endian   x 0.125  ->  real RPM
WRITE  Xterra 0x23D bytes 3-4  LITTLE-endian x 3.125 (raw = rpm / 3.125)
```

- The cluster tach reads **0x23D bytes 3-4, little-endian, RPM = raw x 3.125.**
- The 3.125 scale is the key: an earlier attempt used 0.125 (the common Nissan
  convention), which is **25x too large** -- the cluster decoded "2000 rpm" as
  ~50,000 rpm and ignored it. Same frame, same bytes; only the scale was wrong.
- Providing this correct RPM also fixed the TCCU (it had been reading a frozen
  ~6375 rpm) and unblocked **both 4HI and 4LO**, since valid engine speed is a
  transfer-case permit input.

## Coolant (ECT)

```
0x233 byte0  AND  0x23D byte7   (mirror, must agree)   degC = raw - 50
```

Both bytes carry the same value; the cluster pegs low if they disagree. Currently
a fixed ~90 C placeholder in the gateway -- see Project Status for making it live.

---

## Hardware

| Component        | Purpose  | Notes                                          |
| ---------------- | -------- | ---------------------------------------------- |
| Teensy 4.1       | Core MCU | Native CAN; this project uses CAN1             |
| CAN transceiver  | Bus I/O  | 3.3 V logic (e.g. SN65HVD230); 500 kbit/s      |
| Clean 12V->5V/3V3| Power    | Reverse-polarity + TVS protection recommended  |

Bus speed **500 kbit/s**. When tapping in parallel with the OEM bus (already
terminated at the ECM and a chassis module), the Teensy transceiver should **not**
add another 120 ohm terminator.

---

## Firmware

All sketches: **Teensy 4.1 / FlexCAN_T4**, CAN1, 500 kbit/s. In PlatformIO keep
**only one** program `.cpp` in `src/` at a time (plus any headers it includes).

| Path                                  | Function                                                                  |
| ------------------------------------- | ------------------------------------------------------------------------- |
| `firmware/gateway/gateway.cpp`        | **Working gateway.** Reads Titan 0x180 RPM, writes 0x23D b3-4 (tach + TCCU); presence frames; 0x251 Neutral cycle; ECT mirror (placeholder). |
| `firmware/sniffer_freetext/sniffer_freetext.cpp` | Listen-only logger, free-text markers (type text + Enter). Transmits nothing. |
| `firmware/replay_sweep/`              | Captured-log replay tool (+ `replay_data.h`). Proved the tach signal was in the Frontier capture and bisected to 0x23D. |
| `firmware/tcm_spoof_test/tcm_spoof_test.cpp` | Staged 0x251 TCM gear-status spoof tester.                          |

See **`docs/SIGNALS.md`** for the CAN ID map and **`docs/MASTER_NOTES.md`** for
the full project history and findings.

---

## WARNING: Safety note on the TCM spoof

The `0x251` neutral gear-status cycle exists to clear the Titan ECM's
*lost-communication-with-TCM* code. It is **only appropriate because this is a
manual swap with no automatic transmission present** -- it defeats a check meant
to coordinate with an A/T. **Never use it on a vehicle that has an automatic.**
It does not restore full throttle; the remaining limit is tune-level.

---

## Project status

**Confirmed / working:**
- Single-bus architecture -- ECM (0x1xx) and chassis (0x23x) coexist; verified live.
- **Tach: live, smooth, accurate** -- verified against cluster, ABS, and TCCU RPM readings simultaneously.
  - Read Titan `0x180` b0-1 BE x0.125; write `0x23D` b3-4 LE x3.125.
- **4HI and 4LO both engage** -- the correct RPM signal supplied the TCCU permit input that was missing.
- **Coolant gauge works** -- `0x233` b0 + `0x23D` b7 mirror, degC = raw - 50.
- Chassis comms faults (U1000) cleared by the presence frames.
- `0x251` Neutral cycle clears the ECM lost-TCM comms code (manual swap).

**Open / next:**
- **Live ECT:** coolant is a fixed ~90 C placeholder. To make it track the engine, capture the **Titan ECM** coolant frame (VK56 cold-start warm-up, sniffer + temp markers) and translate to 0x233 b0 / 0x23D b7. The Xterra-side encoding is known; only the Titan source is undecoded. *(The placeholder will not warn of a real overheat -- don't rely on it for temp monitoring until live.)*
- **SES light:** on. Toggling `0x231` toggles it -- that's the thread to pull. May also relate to ECM-side codes from the manual swap.
- **Throttle/torque limit:** tune-level, pending UpRev/Osiris manual-conversion reflash.

**Dead ends / corrections (so others don't repeat them):**
- Tach RPM scale is **3.125, not 0.125** -- the 0.125 convention (correct for the Titan 0x180 side) is 25x wrong for the Xterra 0x23D side.
- Dual-bus physical split -- unnecessary; single bus works.
- `0x23E` is throttle/torque, not RPM.
- TCM CAN emulation does not fix the throttle limit -- that's tune-level.
- The TCCU "frozen 6375 rpm" was the wrong-scale RPM injection, not a TCCU fault.

---

## Repository layout

```
Xterra-Titan-Canbus-Gateway/
├─ README.md
├─ LICENSE
├─ docs/
│  ├─ SIGNALS.md          <- CAN ID map: confirmed vs provisional
│  ├─ GATEWAY_DESIGN.md   <- architecture + how the gateway is built
│  └─ MASTER_NOTES.md     <- full project history and findings
├─ firmware/
│  ├─ gateway/            <- working gateway (live RPM translation)
│  ├─ sniffer_freetext/   <- listen-only logger w/ free-text markers
│  ├─ replay_sweep/       <- captured-log replay + bisection tool
│  └─ tcm_spoof_test/     <- 0x251 TCM spoof tester
├─ data/
│  ├─ id_map.csv          <- machine-readable signal map
│  └─ logs/               <- capture logs (add your own; large files)
└─ scripts/
   └─ convert_savvycan.py <- text log -> CSV for analysis
```

---

## Tools

- **SavvyCAN** for logging/replay; **CoolTerm** (Line Mode) for serial + free-text markers; Topdon scan tool for module data and cross-checking RPM.
- `scripts/convert_savvycan.py` converts sniffer/SavvyCAN text logs to CSV (handles the free-text marker lines too).

## License

MIT -- see [LICENSE](LICENSE).

## Contributing

Doing a similar Nissan CAN swap? Forks and PRs welcome. Goal: make
Titan-into-Xterra (and related F-Alpha / VQ40 / VK56 platform) swaps easier to
document and reproduce.
