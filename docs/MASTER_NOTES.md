# VK56DE -> Xterra Swap: Master Notes

Living summary of the CAN-integration work. Paste/attach at the start of a new
chat (or keep in a Claude Project) to get oriented fast. Keep raw CAN logs in
your own archive; upload a specific log only when analyzing it.

---

## Vehicle configuration
- Chassis: 2005 Nissan Xterra, N50, **manual** (FS6R31A), 4WD.
- Engine/ECM: 2014 Nissan Titan **VK56DE**. 2012 Titan parts truck. 2006 Frontier (VQ40, auto) used as a stock reference for captures.
- Gateway: Teensy 4.1 + FlexCAN_T4, **single CAN bus @ 500 kbit/s**.
- Tools: PlatformIO/VSCode; SavvyCAN; CoolTerm (Line Mode, free-text markers); Topdon scan tool; UpRev/Osiris (owns the tune, no tuner license yet).
- GitHub: https://github.com/Aetoc87/Xterra-Titan-Canbus-Gateway

## Architecture
Single bus. Titan ECM = 0x1xx, Xterra chassis = 0x23x, non-overlapping. Teensy
reads the signals they disagree on (RPM, coolant) and re-broadcasts for the
chassis, plus presence frames. Validated live: cluster, ABS, TCCU all agree.

---

## CONFIRMED / WORKING

- **Tach — live, smooth, accurate.** Read Titan **0x180 b0-1 BE x0.125**; write Xterra **0x23D b3-4 LE, RPM = raw x 3.125**. The 3.125 scale was THE fix (0.125 was 25x too large -> cluster saw ~50,000 rpm and ignored it). Verified against cluster/ABS/TCCU.
- **4HI and 4LO both work.** Correct engine speed on 0x23D gave the TCCU its missing permit input. (Also explained the old "frozen 6375 rpm" TCCU reading — it was the wrong-scale injection.)
- **Coolant gauge — LIVE.** Read Titan **0x551 b0 (DLC 8), degC = raw - 40**; write Xterra **0x233 b0 AND 0x23D b7 (mirror), degC = raw - 50**. Net: **xterra_raw = titan_raw + 10**. Both Xterra bytes must agree or the gauge pegs low.
- **0x551 is load-bearing for VDC (do not remove).** The gateway must keep sending its own **DLC-7 0x551** — VDC/ABS reads something from it; removing it caused a slip light and blocked steering-angle-sensor reset. The Titan ECM also sends 0x551 (DLC 8); the gateway RX reads coolant ONLY from DLC 8 to ignore its own echo. Two senders on 0x551 (gateway DLC7 + Titan DLC8) coexist in practice.
- Titan RPM 0x180 = b0-1 BE x0.125; mirrored 0x1F9 b2-3.
- 0x23E = throttle/torque, not RPM.
- **0x231 drives the SES light.** The SES light is currently gateway-driven (unplug gateway -> light off; an older sketch had it off). Not a real ECM MIL yet.
- 4LO permit (FSM TF-37, manual): stopped, engine running, neutral + clutch + brake. Neutral 4LO switch and wait-detection switch both verified good.

## VEHICLE SPEED — MAPPED (this is the key recent finding)

The Titan ECM has **three** vehicle-speed PIDs, fed by two CAN paths (per Titan FSM P0500):
- **"vehicle speed sensor" PID: WORKS.** Reads real speed from the 2005 chassis (ABS -> combination meter path) while driving. Tracks correctly, zero at stops.
- **"vehicle speed cruise" PID: WORKS**, same source, **clamped to a floor of ~19 mph** (the ASCD minimum-cruise speed). Good sign for the future cruise project.
- **"VEHICLE SPEED" PID: reads 0 ALWAYS.** This is the **TCM-sourced** speed path. No TCM in a manual swap -> permanently zero. No chassis frame feeds it.

So the ECM is NOT speed-blind on the chassis side — it is **TCM-blind**. Earlier "ECM reads zero speed" was this TCM PID.

Note on injection experiments: injecting 0x280/0x284/0x285 (2011-Xterra format) DID move the "vehicle speed sensor" PID (scale differs from the 2011 DBC), and the 2005 cluster consumes 0x284/0x285 (odometer accrued during tests). But injection is unnecessary — the real chassis path already works. The 2005's native speed frame/bytes were NOT decoded (would need a driving sniffer capture); it's moot unless we later need to synthesize speed.

## POWER / TORQUE LIMIT — DIAGNOSED (root cause identified)

WOT pull logs show a **deliberate ECM torque limiter**, not a mechanical/tune-fuel problem:
- At 100% pedal (4.61V) the ECM holds **throttle sensor ~1.8-2.4V** (roughly 30-40% opening), and the opening **increases with RPM at fixed pedal** — the signature of closed-loop torque management metering airflow to a limited torque target.
- Ruled out: **no knock** (HI DET = 0, knock strength normal), **timing normal** (17-23 BTDC under load), **fueling normal** (A/F corrections fine, MAF smooth), rev limit fine (pulled to 5750).
- Root cause points to the **missing TCM**: U0101 stored, TCM speed path dead, and the ECM caps torque because it has no transmission to coordinate with.

**Fix path (in order):**
1. **UpRev retune** (one included with purchase) — sent logs + this diagnosis, asking them to disable TCM-loss torque management for a no-TCM manual app. Question to them: does their tune include TCM-delete/auto-to-manual torque changes or only NATS?
2. If UpRev can't/won't: **full TCM emulation** on the gateway (replicate enough of a real Titan TCM — full 0x251 mux cycle + other TCM frames from parts-truck captures — to satisfy torque arbitration). Bigger project; exhaust UpRev first.
3. Possible: buy the UpRev **tuner license** to inspect/iterate the tune ourselves. Decide after their retune response.

## OPEN / NEXT
- **Torque limit** — awaiting UpRev retune (see above). Re-log a WOT pull after and compare (throttle voltage should chase pedal toward ~4V+ if the limiter is gone).
- **Live SES/MIL** — currently the SES light is falsely gateway-driven. Plan: first make it OFF-by-default (diff the old sketch that had it off vs current, on 0x231), THEN translate the Titan's real MIL bit so the light means something.
- **Live ECT source note:** the Titan coolant frame conflict is handled; ECT is live and done.
- **Cruise control (future):** FSM confirms the manual platform supports cruise with a clutch+neutral cancel path; cruise inputs are mostly hardwired to the ECM (brake, clutch, steering switches) plus CAN speed (which now works). Titan-ECM-specific viability still unknown; revisit after power is sorted.
- **P0462 Fuel Level Sensor** — Titan ECM vs Xterra sender mismatch. Minor, deferred.

## DEAD ENDS / CORRECTIONS
- Tach scale is 3.125 on 0x23D, not 0.125.
- Dual-bus physical split: unnecessary (single bus works). *Caveat:* if the ECM ever needs a CLEAN speed signal without chassis collisions, the Teensy 4.1's 2nd CAN controller (isolate the ECM on its own leg) is the tool — not needed yet.
- 0x23E as RPM: wrong (throttle/torque).
- 0x251 TCM neutral spoof: did NOT clear U0101 in practice -> removed from the gateway.
- TCCU "frozen 6375": was the wrong-scale RPM, not a module fault.
- "ECM reads zero speed" (earlier): that was the TCM-path PID specifically; the chassis speed path works.
- UpRev ROM is encrypted (entropy 8.0) — cannot read/diff the tune without their software.
- The generic tune UpRev supplied did NOT fix the torque limit (confirms it's a specific TCM-torque-management setting, not general).

---

## Sketches (repo)
- `firmware/gateway/gateway.cpp` — production gateway: live RPM (0x180->0x23D 3.125) + live ECT (0x551->0x233/0x23D +10) + presence (incl. required DLC-7 0x551). No spoof, no injection.
- `firmware/sniffer_freetext/sniffer_freetext.cpp` — listen-only logger, free-text markers.
- `firmware/replay_sweep/` — captured-log replay + bisection tool (how the tach frame/scale was found).
- `firmware/tcm_spoof_test/tcm_spoof_test.cpp` — 0x251 spoof tester (kept for reference; spoof not used in the gateway).

## PlatformIO note
Keep exactly ONE program `.cpp` in `src/` at a time (plus its headers). Two program files = "multiple definition of setup/loop" link error.

## Safety
Sniffer is listen-only (safe on a donor vehicle). Speed-injection test sketches are STATIONARY-ONLY (they fight the real ABS while moving) — reflash the clean gateway before driving.
