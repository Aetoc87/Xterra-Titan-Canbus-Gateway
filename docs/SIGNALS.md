# CAN Signal Map

Status: **CONFIRMED** (verified) / **PROVISIONAL** (hypothesis) / **WRONG-DROPPED**
(disproven, kept so others don't repeat it). 11-bit IDs, 500 kbit/s.

---

## Titan ECM side (0x1xx) — read by the gateway

| ID    | Bytes | Signal      | Encoding                    | Status    |
| ----- | ----- | ----------- | --------------------------- | --------- |
| 0x180 | 0-1   | Engine RPM  | **big-endian, x0.125**      | CONFIRMED (gateway reads for tach) |
| 0x1F9 | 2-3   | RPM mirror  | mirrors 0x180               | CONFIRMED |
| 0x551 | 0     | Coolant temp| **DLC 8; degC = raw - 40**  | CONFIRMED (gateway reads for ECT) |

---

## Xterra chassis side (0x23x/0x5xx) — written by the gateway

| ID    | Bytes | Signal | Encoding | Status |
| ----- | ----- | ------ | -------- | ------ |
| **0x23D** | **3-4** | **Engine RPM (cluster tach + TCCU)** | **LE, RPM = raw x 3.125 (raw = rpm/3.125)** | **CONFIRMED** (Frontier corr 1.000; verified live) |
| 0x23D | 7 | Coolant temp (mirror) | degC = raw - 50 | CONFIRMED |
| 0x233 | 0 | Coolant temp | degC = raw - 50 | CONFIRMED (must equal 0x23D b7 or gauge pegs low) |
| 0x551 | 0 | Coolant temp (gateway copy) | degC = raw - 50; **DLC 7** | CONFIRMED — **VDC/ABS requires this frame; do NOT remove** |
| 0x231 | — | engine status; **drives SES light** | toggling toggles SES | PARTIAL (SES currently gateway-driven, not real MIL) |
| 0x23E | — | throttle/torque (NOT rpm) | — | CONFIRMED |
| 0x29E,0x2A5,0x794 | — | presence | working payloads | PROVISIONAL (clear U1000) |

**Tach scale note:** 0x23D uses **3.125** RPM/LSB — NOT the 0.125 convention on the
Titan 0x180 side (that's 25x too large here). This one scale error had blocked the
tach, frozen the TCCU at ~6375 rpm, and prevented 4LO.

**ECT translation:** Titan 0x551 b0 (raw-40) -> +10 -> Xterra 0x233 b0 & 0x23D b7
(raw-50). Gateway RX filters 0x551 to DLC 8 (Titan's) so it ignores its own DLC-7 copy.

---

## Vehicle speed (Titan ECM PIDs) — mapped from UpRev logs

The Titan ECM exposes three vehicle-speed PIDs, fed by two CAN paths (Titan FSM P0500):

| ECM PID | Source path | State in this swap |
| ------- | ----------- | ------------------ |
| "vehicle speed sensor" | ABS -> combination meter (CAN) | **WORKS** — reads real 2005 chassis speed while driving |
| "vehicle speed" (cruise) | same, clamped | **WORKS**, floored at ~19 mph (ASCD min-cruise) |
| "VEHICLE SPEED" | **TCM** (CAN) | **always 0** — no TCM in a manual swap; nothing feeds it |

So the chassis speed path is healthy; the only dead speed path is TCM-sourced.
The 2005's native speed frame/bytes were not decoded (would need a driving sniffer
capture) — moot unless speed must be synthesized later. Xterra 2011 DBC speed IDs
(for reference): 0x280 SPEED (b4-5, 0.01 km/h), 0x284/0x285 wheel speeds — but the
2005's layout differs and the ECM's scale differs from the DBC.

---

## Powertrain / torque

The ~25-40% power cap is a **deliberate ECM torque limiter** (WOT logs: 100% pedal ->
~1.8-2.4V throttle, rising with RPM; no knock, normal timing/fueling). Root cause:
missing TCM (U0101). Fix is tune-level (UpRev TCM-torque-management delete) or full
TCM emulation — not a chassis signal. See MASTER_NOTES.

---

## TCCU / transfer (factory TF section)

TCCU gets vehicle speed + brake (from ABS) and engine speed (from ECM) over CAN.
4LO permit (manual): stopped, engine running, neutral + clutch + brake. **4LO works**
now that the gateway supplies correct engine speed on 0x23D.
Hardwired inputs verified good: Neutral 4LO switch (pin 33 -> F14 p5 -> switch ->
F14 p4 -> ground); wait detection switch (OFF 2WD, ON 4H/4LO).

## Open decode targets
- Real Titan **MIL bit** -> translate to 0x231 so the SES light means something (first make it off-by-default).
- 2005 native vehicle-speed frame (only if speed must be synthesized later).
- Brake/stop-lamp candidate: **0x354 byte6 bit 0x10** (toggled on brake press) — PROVISIONAL; DBC calls 0x354 SPEED_BREAK with a brake-light bit.
