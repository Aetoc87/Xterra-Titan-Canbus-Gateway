# Gateway Design

## Single-bus injector model

The gateway is **not** a two-port bridge. Because the Titan ECM (0x1xx) and the
Xterra chassis modules (0x23x) use non-overlapping ID ranges, both live on one
physical 500 kbit/s bus. The Teensy:

1. **Listens** to everything (for diagnostics/logging).
2. **Injects** the 0x23x-range frames the chassis modules expect but the Titan
   ECM never transmits.

No frames need to be blocked or rewritten between two segments — the chassis
ignores the 0x1xx frames it doesn't recognize, and the ECM ignores the 0x23x
frames it doesn't recognize. The only thing missing on a raw swap is the set of
0x23x "I'm alive / here's engine data" frames, which the gateway supplies.

## Frame scheduling

Each injected frame has its own period in an explicit **schedule table**:

```c
struct SchedFrame {
  uint32_t id;
  uint8_t  len;
  uint8_t  data[8];
  uint16_t period_ms;
  uint32_t last_ms;
  bool     hasCounter;   // byte0 replaced with advancing counter
};
```

The loop walks the table and sends any frame whose period has elapsed. This
replaced an earlier approach that derived a transmit slot from `id % 64`, which
risked two IDs hashing to the same slot (a slot-collision bug). The explicit
table is collision-free and easy to maintain — to add or retime a frame you edit
one row.

## Counters

Some chassis modules expect a **changing** frame, not a static one. Two places
use an advancing counter:

- **0x23D** (engine data): byte0 advances each send. Providing this live-looking
  counter is what enabled 2WD↔4HI shifting.
- **0x251** (TCM neutral spoof): byte0/byte5 advance to mimic the Titan TCM's
  rolling counter.

## RPM translation (confirmed)

The gateway reads the Titan ECM's engine speed and rewrites it for the chassis:

- **Read** Titan `0x180` bytes 0-1, big-endian, x0.125 -> real RPM (RX callback).
- **Write** `0x23D` bytes 3-4, little-endian, `raw = rpm / 3.125` -> the value the
  cluster tach and the TCCU read. The 3.125 scale is essential; 0.125 (the Titan-side
  convention) is 25x too large here and the cluster ignores it.

If no `0x180` is seen for 500 ms (ECM off/disconnected) the tach falls to 0 rather
than freezing on a stale value.

## ECT translation (live)

The gateway reads the Titan ECM's coolant from **0x551 byte0 (DLC 8), degC =
raw - 40**, and writes it to the Xterra cluster on **0x233 b0 AND 0x23D b7
(mirror), degC = raw - 50** — a net **+10** offset on the raw byte. The RX path
filters 0x551 to DLC 8 so the gateway reads the Titan's frame, not its own DLC-7
copy (see below). If the Titan 0x551 goes stale for 2 s, ECT falls back to ~90 C.

## 0x551 is required by VDC (do not remove)

The gateway sends its own **DLC-7 0x551** presence frame with coolant in byte0.
The VDC/ABS reads something from this frame — removing it causes a slip light and
blocks steering-angle-sensor reset. Both the gateway (DLC 7) and the Titan ECM
(DLC 8) transmit 0x551; they coexist in practice, and the DLC difference lets the
gateway's RX ignore its own echo.

## What's still provisional

- The presence-frame payloads are working values, but most are not individually
  decoded — they satisfy the chassis comms checks, not necessarily carrying
  meaningful signal content.
- The SES light (0x231) is currently driven by the gateway rather than a real
  ECM MIL bit — to be made off-by-default then wired to the real MIL.

## Safety boundaries

- The **TCM spoof is manual-swap-only.** It defeats an A/T-coordination check and
  must never be used with an automatic present. See README.
- The sniffer (`sniffer_freetext`) is **listen-only** and transmits nothing —
  safe to use on another vehicle (e.g. a donor truck) for captures.
