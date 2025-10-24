# 🛠️ Xterra ↔ Titan CAN Bus Gateway (Teensy 4.1 Edition)

> A dual-bus CAN gateway that allows a **2nd-Gen Nissan Xterra** chassis to operate with a **Nissan Titan ECM and drivetrain**.  
> Built around a **Teensy 4.1** microcontroller and dual **SN65HVD230** CAN transceivers.

---

## ⚙️ Project Goals

- Integrate a **Titan VK56DE ECM** with the **Xterra chassis** electronics.  
- Emulate or forward key CAN messages (RPM, MIL, steering angle, etc.) required by ABS, VDC, and other modules.  
- Maintain OEM-style reliability, termination, and signal integrity.  
- Provide open, well-documented firmware for other Nissan CAN swap enthusiasts.

---

## 🧩 Hardware Overview

| Component | Purpose | Notes |
|------------|----------|-------|
| **Teensy 4.1** | Core MCU | 3 native CAN controllers, used here with CAN1 + CAN2 |
| **SN65HVD230 (Waveshare)** ×2 | CAN transceivers | 3.3 V logic, on-board 120 Ω resistor |
| **Power Supply** | 12 V → 5 V/3.3 V | Use clean regulated power with reverse-polarity + TVS protection |

### Pin assignments (Teensy 4.1)
| Bus | CAN TX | CAN RX | Transceiver |
|------|--------|--------|--------------|
| CHASSIS | 22 | 23 | SN65HVD230 #1 |
| POWERTRAIN | 1 | 0 | SN65HVD230 #2 |

---

## 🔌 CAN Bus Topology

The factory CAN line between the **IPDM** and **ECM** is **split near the firewall**, isolating the two networks:
[Cluster, ABS, TCCM, IPDM] --120Ω-- [SN65HVD230 #1] ⇄ CAN1 ⇄ Teensy 4.1 ⇄ CAN2 ⇄ [SN65HVD230 #2] --120Ω-- [ECM, IPDM lead]


- **CHASSIS bus:** all body modules remain connected to IPDM and terminate at the IPDM and gateway transceiver.  
- **POWERTRAIN bus:** the Titan ECM and IPDM harness stub form a second segment, terminated at the ECM and the gateway transceiver.  

This creates **two properly terminated 60 Ω CAN networks**, bridged in software by the Teensy.

---

## 📡 Termination & Wiring Notes

| Mode | SN65HVD230 Resistor | Description |
|------|----------------------|-------------|
| **Sniffer / injector** | ❌ *Removed* | When connected in parallel to the OEM bus (ECM + IPDM handle termination) |
| **Gateway (split-bus)** | ✅ *Installed* | Each SN65HVD230 acts as one physical end of its bus segment |

**Grounding:** tie both transceiver grounds and the Teensy GND together, and bond to vehicle chassis at a single point.  
Use twisted pairs for CANH/CANL (≈ 120 Ω impedance) and keep leads under ~20 cm to the splice points.

---

## 🧰 Firmware Overview

Firmware is written for **Teensy 4.1** using the **FlexCAN_T4** library.

### Main sketches
| Path | Function |
|------|-----------|
| `firmware/gateway_teensy41/gateway_teensy41.ino` | Dual-bus gateway core (bridges chassis ↔ powertrain) | WIP
| `firmware/utils/can_sniffer.ino` | Passive CAN sniffer for logging / ID discovery |
| `firmware/test_injection/test_injection.ino` | Injector used to reproduce and clear communication faults during testing |

Typical bus speed: **500 kbit/s** on both networks.

---

## 📂 Repository Layout
xterra-titan-canbus-gateway/
├─ README.md
├─ LICENSE
├─ docs/
│ ├─ Gateway-Design.md
│ ├─ Logs-And-Sketches.md
│ └─ ...
├─ firmware/
│ ├─ gateway_teensy41/
│ ├─ test_injection/
│ └─ utils/
├─ data/
│ ├─ logs/ ← your real CAN log files
│ └─ id_map.csv ← signal definitions
└─ scripts/
└─ convert_savvycan.py


---

## 🧾 CAN Messages of Interest

| ID | Direction | Suspected Purpose | Notes / Status |
|----|------------|------------------|----------------|
| `0x180` | ECM → Chassis | Possible Engine RPM | Seen changing during rev sweep; not verified |
| `0x29E` | ECM → Chassis | Possible ECM status / MIL | Appears when MIL active; under review |
| `0x160` | ECM → Chassis | Possibly torque/load or throttle data | Frequently active; contents vary with throttle |
| `0x6F7` | ECM → Chassis | Diagnostic or heartbeat | Low-frequency, constant frame |
| `0x182`, `0x285` | ECM ↔ ABS/TCCM | Unknown | Observed during motion and fault logging; analysis ongoing |

**Summary:**  
No frame has yet been confirmed to carry RPM, steering angle, or VDC handshake data.  
Identification efforts are ongoing using rev-sweep, idle, and ABS fault logs.

---

## 🧮 Tools

- **SavvyCAN** or **BUSMaster** for logging / replay  
- **convert_savvycan.py** (in `/scripts`) — converts text logs into CSVs for analysis:
  ```bash
  python3 scripts/convert_savvycan.py data/logs/rev_sweep.txt data/logs/rev_sweep.csv

🧑‍🔬 Testing Progress

✅ CAN communication restored between ABS and TCCM after synthetic frame injection

⚙️ Steering angle signal still under investigation (C1156 VDC code)

🧱 Rear diff lock module ignored (planned bypass due to U1000 Communication code)

🧩 Next: verify ECM ↔ gateway isolation and message bridging logic

🧱 Planned Enhancements

Add filtering & translation rules in firmware (e.g., RPM scaling, MIL relay)

Automatic CAN bus diagnostics on startup

Bench simulation mode (inject + log without vehicle connection)

Expand ID map for cluster and VDC messages

⚖️ License

MIT License © 2025 Xterra-Titan CAN Gateway Contributors

🤝 Community

This project is open for collaboration!
If you’re doing a similar Nissan swap or reverse-engineering CAN messages, feel free to fork and submit pull requests.

Let’s make Titan drivetrain swaps easier for every Xterra owner.
