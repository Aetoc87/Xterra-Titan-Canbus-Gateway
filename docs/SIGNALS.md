# CAN Signal Map

Status: **CONFIRMED** (verified) / **PROVISIONAL** (hypothesis) / **WRONG-DROPPED**
(disproven, kept so others don't repeat it). 11-bit IDs, 500 kbit/s.

---

## Titan ECM side (0x1xx) — read by the gateway

| ID    | Bytes | Signal     | Encoding                       | Status    |
| ----- | ----- | ---------- | ------------------------------ | --------- |
| 0x180 | 0-1   | Engine RPM | **big-endian, x0.125**         | CONFIRMED (rev sweep; gateway reads this) |
| 0x1F9 | 2-3   | RPM mirror | mirrors 0x180                  | CONFIRMED |
| 0x251 | see below | TCM gear status | bitfield                  | CONFIRMED (parts-truck capture) |

### 0x251 — Titan TCM gear status
DLC 8, ~100 Hz. byte3 bitfield: 0x01=Park, 0x02=Reverse, **0x04=Neutral**, 0x08=Drive.
byte0/byte5 rolling counter; byte6 mux index; byte7 mux payload (not a live checksum).
Gateway replays the verbatim 12-frame Neutral cycle to clear the ECM lost-TCM code
(manual swap only).

---

## Xterra chassis side (0x23x) — written by the gateway

| ID    | Bytes | Signal | Encoding | Status |
| ----- | ----- | ------ | -------- | ------ |
| **0x23D** | **3-4** | **Engine RPM (cluster tach + TCCU)** | **little-endian, RPM = raw x 3.125 (raw = rpm/3.125)** | **CONFIRMED** (Frontier capture corr 1.000; verified live on cluster/ABS/TCCU) |
| 0x23D | 7 | Coolant temp (mirror) | degC = raw - 50 | CONFIRMED |
| 0x233 | 0 | Coolant temp | degC = raw - 50 | CONFIRMED (must match 0x23D b7 or gauge pegs low) |
| 0x231 | — | engine status; **drives SES light** | toggling 0x231 toggles SES | PARTIAL (SES link confirmed; full decode TODO) |
| 0x23E | — | throttle/torque (NOT rpm) | — | CONFIRMED (zero at engine-off) |
| 0x29E,0x2A5,0x551,0x794 | — | presence | working payloads | PROVISIONAL (clear U1000) |

**Tach scale note:** the cluster uses **3.125** RPM/LSB on 0x23D — NOT the 0.125
convention used on the Titan 0x180 side. Using 0.125 here is 25x too large and the
cluster ignores it. This single scale error had blocked the tach, frozen the TCCU
at ~6375 rpm, and prevented 4LO.

**Gateway translation:** read Titan 0x180 (b0-1 BE x0.125) -> RPM -> write
0x23D b3-4 (LE, /3.125). Drives cluster tach and feeds the TCCU/ABS a valid
engine speed.

---

## TCCU / transfer (factory TF section)

TCCU receives over CAN: vehicle speed (from ABS), stop-lamp/brake (from ABS),
engine speed (from ECM). 4LO permit (manual): vehicle stopped, engine running,
neutral + clutch + brake. **4LO now works** once the gateway supplies correct
engine speed on 0x23D — that was the missing permit input.

Hardwired inputs verified good: Neutral 4LO switch (TCCU pin 33 -> F14 p5 ->
switch -> F14 p4 -> ground); wait detection switch (OFF in 2WD, ON in 4H/4LO).

---

## Open decode targets

- **Titan ECM coolant frame** — to make ECT live (Xterra side known; Titan source undecoded). Needs a VK56 cold-start warm-up capture.
- **0x231 full decode** — for the SES light.
- Brake/stop-lamp candidate: **0x354 byte6 bit 0x10** (toggled on brake press in Frontier capture) — PROVISIONAL.
