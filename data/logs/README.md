# Logs

Place raw CAN capture logs here (sniffer text output or SavvyCAN exports).

Suggested naming:
- `titan_rev_sweep.txt`        — Titan VK56 RPM sweep (0x180 confirmed here)
- `titan_tcm_neutral.txt`      — parts-truck TCM 0x251 neutral capture
- `koeo_baseline.txt`          — key-on engine-off rest state
- `frontier_rev_sweep.txt`     — stock VQ40 rev sweep (tach target)
- `frontier_coldstart.txt`     — stock VQ40 cold-start warm-up (coolant temp)
- `frontier_brake.txt`         — brake press/release
- `frontier_speed.txt`         — 0->30->0 km/h roll

Convert to CSV for analysis:
```
python3 ../../scripts/convert_savvycan.py frontier_rev_sweep.txt frontier_rev_sweep.csv
```

Large logs are fine. If you prefer to keep big binaries out of git history,
add them to `.gitignore` and attach to a release instead.
