# VK56DE -> Xterra Swap: Master Notes

Living summary of the CAN-integration work. Paste/attach at the start of a new
chat (or keep in a Claude Project) to get oriented fast. Keep raw CAN logs in
your own archive; upload a specific log only when analyzing it.

---

## Vehicle configuration
- Chassis: 2005 Nissan Xterra, N50, **manual** (FS6R31A), 4WD.
- Engine/ECM: 2014 Nissan Titan **VK56DE**. 2012 Titan parts truck. 2006 Frontier (VQ40, auto) used as a stock reference for captures.
- Gateway: Teensy 4.1 + FlexCAN_T4, **single CAN bus @ 500 kbit/s**.
- Tools: PlatformIO/VSCode; SavvyCAN; CoolTerm (Line Mode, free-text markers); Topdon scan tool; UpRev/Osiris for tuning.
- GitHub: https://github.com/Aetoc87/Xterra-Titan-Canbus-Gateway

## Architecture
Single bus. Titan ECM = 0x1xx, Xterra chassis = 0x23x, non-overlapping. Teensy
reads the one shared-meaning signal (RPM) and re-broadcasts it for the chassis,
plus presence frames. Validated live: cluster, ABS, TCCU all read the same RPM
with ECM + gateway both on the bus.

---

## CONFIRMED / WORKING

- **Tach — live, smooth, accurate.** Verified against cluster, ABS, and TCCU simultaneously.
  - Read Titan **0x180 b0-1 BE x0.125** -> RPM.
  - Write Xterra **0x23D b3-4 LE, RPM = raw x 3.125 (raw = rpm/3.125)**.
  - The 3.125 scale was THE fix. Prior 0.125 attempt was 25x too large -> cluster saw ~50,000 rpm and ignored it. Frame and bytes were right all along; only scale was wrong.
- **4HI and 4LO both work.** Supplying correct engine speed on 0x23D gave the TCCU the permit input it lacked. (This also explained the long-standing "frozen 6375 rpm" on the TCCU — it was the wrong-scale injection, not a TCCU fault.)
- **Coolant gauge works.** ECT on **0x233 b0 AND 0x23D b7 (mirror), degC = raw - 50.** Both must agree or the gauge pegs low.
- Chassis comms (U1000) cleared by presence frames.
- 0x251 verbatim 12-frame Neutral cycle clears the ECM lost-TCM comms code (manual swap).
- Titan RPM 0x180 = b0-1 BE x0.125 (rev-sweep confirmed); mirrored 0x1F9 b2-3.
- 0x23E = throttle/torque, not RPM.
- Frontier decode anchors: 0x23D b3-4 LE tracked RPM at corr 1.000 (scale 3.125); coolant 0x233 b0 / 0x23D b7 degC=raw-50 (fit to scan-tool temp markers).
- **0x231 drives the SES light** (toggling it toggles the lamp).
- 4LO permit (FSM TF-37, manual): stopped, engine running, neutral + clutch + brake. Neutral 4LO switch circuit and wait-detection switch both verified good.

## OPEN / NEXT

- **Live ECT:** gateway currently sends a fixed ~90 C placeholder. Need a **Titan VK56 cold-start warm-up capture** (sniffer + temp markers) to find the Titan's coolant frame/byte/scale, then translate to 0x233 b0 / 0x23D b7. Xterra side known; Titan source undecoded. Placeholder won't warn of real overheat.
- **SES light:** on. 0x231 is the lead. May also tie to ECM-side codes from the manual swap.
- **Throttle/torque limit (~25%):** tune-level, pending UpRev/Osiris manual-conversion reflash. (Confirm UpRev supports the specific 2014 VK56DE part number.)

## DEAD ENDS / CORRECTIONS
- Tach scale is 3.125 on 0x23D, not 0.125 (0.125 is correct only for Titan 0x180).
- Dual-bus physical split: unnecessary.
- 0x23E as RPM: wrong (throttle/torque).
- TCM emulation to fix throttle: won't; tune-level.
- TCCU "frozen 6375": was the wrong-scale RPM, not a module fault.
- Neutral switch / wait-detection switch as 4LO blockers: both verified good; the real blocker was the missing/garbage engine-speed signal.

---

## Sketches (repo)
- `firmware/gateway/gateway.cpp` — working gateway: live Titan 0x180 RPM -> 0x23D b3-4; presence; 0x251 Neutral; ECT mirror (placeholder ~90C).
- `firmware/sniffer_freetext/sniffer_freetext.cpp` — listen-only logger, free-text markers. Transmits nothing.
- `firmware/replay_sweep/` — captured-log replay + group/ID bisection tool (how the tach frame was found). Includes replay_data.h (Frontier sweep slice).
- `firmware/tcm_spoof_test/tcm_spoof_test.cpp` — 0x251 spoof tester.

## How the tach was cracked (method worth reusing)
1. Individual frame injections (0x1F9, 0x23D, 0x231, 0x253) all failed -> dead end by guessing.
2. **Replayed the whole Frontier capture** -> tach moved -> proved the signal IS in the capture.
3. Bisected by ID group, then per-ID -> isolated **0x23D**.
4. Realized 0x23D-with-captured-bytes worked but our-content didn't -> **scale error**, not frame error.
5. Correlated 0x23D bytes vs known RPM -> b3-4 LE, **x3.125** (corr 1.000).

## PlatformIO note
Keep exactly ONE program `.cpp` in `src/` at a time (plus its headers). Two
program files = "multiple definition of setup/loop" link error.

## Safety
0x251 TCM spoof is manual-swap-only (defeats an A/T-coordination check). Sniffer
is listen-only, safe on a donor vehicle.
