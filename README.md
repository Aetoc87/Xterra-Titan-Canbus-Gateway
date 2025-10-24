# Xterra ↔ Titan CAN Bus Gateway (Teensy 4.1 Edition)

> CAN gateway allowing a 2nd-gen Xterra chassis to communicate with a Titan drivetrain/ECM.

## ⚙️ Hardware

- **MCU:** Teensy 4.1  
  - CAN1 → CHASSIS bus (pins 22 TX, 23 RX)  
  - CAN2 → POWERTRAIN bus (pins 1 TX, 0 RX)
- **Transceivers:** Two SN65HVD230 (Waveshare)
- **Termination:**
  - Sniffer mode → remove onboard 120 Ω
  - Gateway mode → keep both 120 Ω enabled

### Bus Split Topology
```
[Cluster, ABS, IPDM] --120Ω-- [SN65HVD230 #1]⇄CAN1⇄Teensy⇄CAN2⇄[SN65HVD230 #2]--120Ω--[ECM]
```
