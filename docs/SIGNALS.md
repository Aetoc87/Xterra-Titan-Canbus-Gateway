# CAN Signal Map

Status legend:
- **CONFIRMED** — verified against first-party captures and/or the factory service manual.
- **PROVISIONAL** — working hypothesis; not yet verified.
- **WRONG/DROPPED** — previously assumed, since disproven (kept so others don't repeat it).

All IDs are 11-bit standard. Bus is 500 kbit/s.

---

## Titan ECM side (0x1xx range)

| ID    | Bytes | Signal           | Encoding                         | Status    |
| ----- | ----- | ---------------- | -------------------------------- | --------- |
| 0x180 | 0–1   | Engine RPM       | big-endian, 0.125 RPM/LSB (÷8)   | CONFIRMED (rev sweep) |
| 0x1F9 | 2–3   | Engine RPM (mirror) | mirrors 0x180 RPM             | CONFIRMED |
| 0x160 | —     | Engine load / torque-ish | varies with throttle     | PROVISIONAL (observed) |
| 0x182 | —     | (unknown)        | —                                | PROVISIONAL |
| 0x251 | see below | TCM gear status | see TCM section               | CONFIRMED (parts-truck capture) |

### 0x251 — Titan TCM gear status (CONFIRMED)
- DLC 8, ~100 Hz (10 ms).
- **byte3 = gear bitfield:** `0x01`=Park, `0x02`=Reverse, `0x04`=Neutral, `0x08`=Drive.
  (Park and Neutral marker-confirmed. In Drive, byte1 also reads `0x10`.)
- **byte0 / byte5 = rolling counter.**
- **byte6 = multiplex index; byte7 = multiplexed payload** (NOT a live checksum).
- A frozen neutral replay clears the ECM lost-TCM comms code (U1000/U1001) in the
  manual swap. Does NOT restore throttle (tune-level limit remains).

---

## Xterra VQ40 chassis side (0x23x range)

The chassis modules (combination meter, ABS, TCCU/transfer) expect these. Absence
sets U1000 "lost communication" codes. The gateway injects them.

| ID    | Signal (per 2011 Xterra DBC / FSM) | Status    | Notes                                   |
| ----- | ---------------------------------- | --------- | --------------------------------------- |
| 0x231 | ENGINE_2                           | PROVISIONAL | presence payload working                |
| 0x233 | ENGINE_7                           | PROVISIONAL | presence                                |
| 0x23D | ENGINE_3 — engine speed (TCCU reads this) | PARTIAL | presence + advancing counter enables 4HI; **RPM scale NOT confirmed** — TCCU reads frozen/implausible value |
| 0x23E | ENGINE_4 — **throttle/torque, NOT RPM** | CONFIRMED | all-zeros at engine-off confirmed it's not RPM |
| 0x1F9 | ENGINE_1 — RPM 0.125                | CONFIRMED | (shared convention with Titan side)     |
| 0x253 | TCU_1                              | PROVISIONAL | —                                       |
| 0x551 | ENGINE_5                           | PROVISIONAL | presence                                |
| 0x580 | ENGINE_6                           | PROVISIONAL | tach candidate (under test)             |
| 0x29E | (presence)                         | PROVISIONAL | clears comms when present               |
| 0x2A5 | (presence)                         | PROVISIONAL | clears comms when present               |
| 0x794 | (presence)                         | PROVISIONAL | clears comms when present               |

---

## TCCU / transfer (from factory TF service-manual section)

The transfer control unit (TCCU) receives, **over CAN**:
- **Vehicle speed** — from the ABS actuator/electric unit (CAN). 4LO permit requires this to read 0.
- **Stop-lamp / brake signal** — from ABS (CAN). Part of the manual 4LO permit.
- **Engine speed** — from the ECM (CAN). Monitored as ">400 rpm = running"; no documented RPM ceiling in the 4LO permit table.

Hardwired (not CAN) inputs verified on the build:
- **Neutral 4LO switch** — TCCU pin 33 → connector F14 pin 5 → switch → F14 pin 4 → ground. Circuit confirmed good (pin 33 pulls to ground in neutral).
- **Wait detection switch** — OFF in 2WD, ON in 4H and 4LO-attempt. Confirmed normal per FSM TF-37.

**4LO permit conditions (manual), per FSM TF-37:**
vehicle stopped • engine running • M/T in neutral with **clutch AND brake depressed**.

---

## Open decode targets (Frontier capture session)

To be resolved from a stock 2006 Frontier (VQ40, same platform) capture:
- **Engine-speed frame the cluster/TCCU actually read** — rev-sweep capture; find the 0x23x frame/byte/endianness/scale that tracks real RPM. Fixes the tach and the TCCU frozen-engine-speed artifact.
- **Coolant temp byte/scale** — cold-start warm-up capture; find the byte that ramps with temperature, anchored to noted temps.
- **Vehicle-speed / brake frames** at true zero and while moving — confirm the TCCU permit inputs.
