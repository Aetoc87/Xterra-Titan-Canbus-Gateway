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

## What's deliberately provisional

- The **RPM value** encoded into 0x23D is not yet confirmed correct. The frame's
  presence + counter satisfies the chassis enough for 4HI, but the TCCU reads a
  frozen/implausible engine speed from the injected value. The byte/scale will be
  corrected once the stock-VQ40 capture identifies the real engine-speed encoding.
  Until then, treat the 0x23D RPM bytes as a placeholder.

- The presence-frame payloads are working values, but most are not individually
  decoded — they're known to satisfy the chassis comms checks, not necessarily to
  carry meaningful signal content.

## Safety boundaries

- The **TCM spoof is manual-swap-only.** It defeats an A/T-coordination check and
  must never be used with an automatic present. See README.
- The sniffer (`sniffer_freetext`) is **listen-only** and transmits nothing —
  safe to use on another vehicle (e.g. a donor truck) for captures.
