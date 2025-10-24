# Gateway Design (Teensy 4.1 + SN65HVD230)

Split the CAN line near the ECM firewall:
- CHASSIS bus: IPDM ↔ Gateway (CAN1)
- POWERTRAIN bus: Gateway (CAN2) ↔ ECM

Each segment terminated with 120 Ω at both ends (≈60 Ω across CANH–CANL).
