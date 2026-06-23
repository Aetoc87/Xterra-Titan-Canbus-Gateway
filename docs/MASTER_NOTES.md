# VK56DE → Xterra Swap: Master Notes

A living summary of the CAN-integration work. Paste or attach this at the start
of a new chat (or keep it in your Claude Project) to get oriented fast. Keep the
raw CAN logs in your own archive / GitHub, not in the project knowledge — upload
a specific log only when actively analyzing it.

---

## Vehicle configuration

- **Chassis:** 2005 Nissan Xterra, N50, **manual transmission** (FS6R31A), 4WD.
- **Engine/ECM:** 2014 Nissan Titan **VK56DE** ECM. Also have a 2012 Titan parts truck.
- **Gateway hardware:** Teensy 4.1 + FlexCAN_T4, single CAN bus @ 500 kbit/s.
- **Toolchain:** PlatformIO/VSCode; SavvyCAN; Topdon scan tool; Consult II/III; UpRev/Osiris for tuning.
- Both vehicles are close together for fast iterative testing.
- GitHub: https://github.com/Aetoc87/Xterra-Titan-Canbus-Gateway

---

## Architecture (the key insight)

**Single bus, no physical split needed.** Titan ECM broadcasts on **0x1xx**;
Xterra chassis modules listen on **0x23x**. Ranges don't overlap, so both share
one bus and the Teensy just **injects** the 0x23x frames the chassis expects but
the Titan ECM never sends. The gateway is an injector/translator, not a two-port
bridge. (Earlier dual-bus-split design was dropped as unnecessary.)

---

## Confirmed findings

- **Titan RPM:** `0x180` bytes 0–1, big-endian, **0.125 RPM/LSB (÷8)**. Rev-sweep verified. Mirrored on `0x1F9` bytes 2–3.
- **VQ40 engine IDs broadcast ~39 Hz** (not the 8.5/10 Hz earlier guessed).
- **`0x23E` = throttle/torque, NOT RPM.** Confirmed by all-zeros at engine-off. (The earlier "0x23E = RPM" idea came from a discarded third-party PCAN log and was wrong.)
- **TCM gear frame `0x251`** (from parts-truck capture): DLC 8, ~100 Hz. **byte3 bitfield** P=0x01, R=0x02, **N=0x04**, D=0x08 (Park/Neutral marker-confirmed; Drive also sets byte1=0x10). byte0/byte5 = rolling counter. byte6 = mux index, byte7 = mux payload (not a live checksum).
- **Presence frames** (0x29E, 0x2A5, 0x231, 0x233, 0x23D, 0x23E, 0x551, 0x794) injected → clears chassis U1000 lost-comm codes.
- **2WD↔4HI works** once `0x23D` is provided with an advancing byte0 counter.
- **`0x251` neutral replay clears the ECM lost-TCM comms code** (manual swap). Does NOT restore throttle.
- **4LO permit (manual), per FSM TF-37:** vehicle stopped • engine running • neutral + **clutch AND brake** depressed. Vehicle speed reaches TCCU via CAN from ABS; engine speed via CAN from ECM (no dedicated wire). Engine speed only checked as ">400 rpm = running," no documented ceiling.
- **Neutral 4LO switch circuit verified good:** TCCU pin 33 → F14 pin 5 → switch → F14 pin 4 → ground; pin 33 pulls to ground in neutral.
- **Wait detection switch verified normal:** OFF in 2WD, ON in 4H and 4LO-attempt (matches FSM TF-37). So it's not the 4LO blocker.
- **Rear axle / brakes platform note:** 2nd-gen Xterra uses 6×114.3 mm bolt pattern vs Titan's 6×139.7 mm — incompatible for big-brake/axle swaps; pattern is machined into the semi-floating rear axle shaft flanges, so no simple hub swap.

---

## Open / in progress

- **Tach + TCCU engine speed:** the exact 0x23x frame/byte/endianness/scale the cluster and TCCU read for engine speed is **not confirmed**. The TCCU currently reads a **frozen ~6375 RPM** from the current gateway sketch regardless of injection settings — an artifact of the sketch (it read 0 before this sketch). Likely the value is coming from a constant byte / wrong frame, i.e. the TCCU isn't reading our intended 0x23D RPM bytes. **Pending Frontier rev-sweep capture** to find the real encoding; then fix the gateway and the 6375 artifact together. Probably NOT the 4LO blocker, but worth fixing.
- **Coolant temp:** cluster temp-gauge byte/scale not decoded. **Pending Frontier cold-start warm-up capture** (note actual temps at timestamps to anchor scale).
- **4LO engagement:** still won't engage — motor is **not commanded** (silent), so it's a permit condition, not mechanical/actuator (4HI motor sounds strong). Remaining suspects: engine-speed signal being wrong/frozen; the **4LO switch** circuit (L-H fork position) on the heavily-rewired F14, **not yet verified** the way the neutral switch was. Confirm clutch+brake+neutral all held during attempt.
- **Throttle/torque limit:** ECM limits to ~25% even with comms codes cleared. **Tune-level** — pursuing UpRev/Osiris manual-conversion reflash (UpRev contacted; confirm they support the specific 2014 VK56DE part number). Note: a 2012 Titan ECM would NOT fix tach/RPM (same 0x1xx-vs-0x23x mismatch) but might be better-supported by UpRev as a fallback.

---

## Dead ends / corrections (don't repeat)

- Dual-bus physical split — unnecessary; single bus works.
- `0x23E` as RPM — wrong (throttle/torque).
- TCM CAN emulation to fix throttle — won't; the limit is tune-level.
- 2012/2013 "VK56 break" as applied to Titan — wrong; Titan kept VK56DE through 2015. The DI/VVEL VK56VD change was Armada/Infiniti ~2010.
- Neutral switch and wait-detection switch as 4LO blockers — both verified good.

---

## Sketches (in the repo)

- **`firmware/gateway/gateway.ino`** — working gateway: presence frames + 0x23D engine data (advancing counter) + 0x251 neutral TCM spoof. Clears comms, enables 4HI. (0x23D RPM scale is a placeholder pending capture.)
- **`firmware/sniffer_freetext/sniffer_freetext.ino`** — listen-only logger, free-text markers (type text + Enter). For the Frontier capture session. Transmits nothing.
- **`firmware/tcm_spoof_test/tcm_spoof_test.ino`** — staged 0x251 spoof tester (modes A/B/C, gear toggle, rate).
- **`firmware/tach_target_test/tach_target_test.ino`** — RPM-injection tester (targets 0x23D/0x1F9/0x580; sweep or fixed; adjustable scale).

---

## Next concrete step

**Frontier (stock 2006 VQ40, automatic) capture session.** Listen-only sniffer,
OBD pins 6 (CAN-H)/14 (CAN-L) @ 500k, separate named files, free-text markers.
Priorities:
1. **Cold-start warm-up** (highest — only obtainable once; engine cold once per visit). Note coolant temps at timestamps. → coolant gauge.
2. **Rev sweep** idle→3000→idle. → tach target + TCCU engine-speed feed.
3. Brake press/release. → 4LO permit brake input.
4. Short 0→30→0 roll (can drive). → vehicle-speed/brake confirm.
5. KOEO baseline. → rest-state reference to subtract against.

It's an automatic — note that when sending logs (rear wheel speed routes via TCM
on A/T). Wheel speed / speedo already work on the build, so vehicle-speed decode
is confirmation, not critical.

---

## Safety boundary

The 0x251 TCM spoof is **manual-swap-only** — it defeats an A/T-coordination
check and must never be used with an automatic present. The sniffer is
listen-only and safe on a donor vehicle.
