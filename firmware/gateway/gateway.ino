/*
 * Xterra <-> Titan CAN Gateway   Teensy 4.1 + FlexCAN_T4   single bus @ 500k
 * ==========================================================================
 * WORKING gateway sketch as of the current project state. This is the build
 * that clears the chassis U1000/ABS/TCCM communication faults, makes 2WD<->4HI
 * shifting work, and keeps the Titan ECM out of TCM limp-related comms codes.
 *
 * ---------------------------------------------------------------------------
 * ARCHITECTURE (single bus -- no physical split needed)
 * ---------------------------------------------------------------------------
 * The Titan VK56DE ECM broadcasts on the 0x1xx ID range. The Xterra VQ40
 * chassis modules (cluster, ABS, TCCU/transfer) listen on the 0x23x range.
 * The two ranges DO NOT overlap, so the ECM and chassis can share one physical
 * bus and the Teensy simply ADDS the frames the chassis expects but the Titan
 * ECM never sends. No bus split, no second transceiver required.
 *
 * This sketch is an INJECTOR/translator on one bus, not a two-port bridge.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS SKETCH SENDS
 * ---------------------------------------------------------------------------
 * 1. PRESENCE FRAMES: a set of 0x23x/0x5xx/0x7xx frames the chassis modules
 *    expect to see periodically. Their absence sets U1000 "lost comm" codes.
 *    Sent on a fixed per-frame schedule (see scheduleTable below).
 *
 * 2. 0x23D ENGINE DATA: carries an advancing byte0 counter. Providing this
 *    (with a live-looking counter) is what got 2WD<->4HI shifting to work.
 *    NOTE: the RPM value encoding in this frame is NOT yet confirmed correct
 *    -- the TCCU currently reads a frozen/implausible engine speed from the
 *    injected data. Pending Frontier (stock VQ40) capture to confirm the real
 *    engine-speed frame/byte/scale. See docs/SIGNALS.md.
 *
 * 3. 0x251 TCM GEAR SPOOF (NEUTRAL): a frozen single-frame replay of the
 *    Titan TCM's neutral gear-status frame. Clears the ECM's lost-TCM comms
 *    code (U1000/U1001) in this manual-swap application where no automatic
 *    TCM exists.
 *      *** SAFETY: this spoof is ONLY appropriate because there is NO
 *      automatic transmission present. It defeats a check that exists to
 *      coordinate with an A/T. Do NOT use on a vehicle that has an automatic.
 *      It does NOT restore full throttle authority -- the remaining torque
 *      limit is tune-level (UpRev/Osiris), not a comms problem. ***
 *
 * ---------------------------------------------------------------------------
 * SCHEDULE TABLE
 * ---------------------------------------------------------------------------
 * Each frame has its own period. This replaces an earlier id%64 slot-assignment
 * approach that risked slot collisions; the explicit table is maintainable and
 * collision-free.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
static const uint32_t CAN_BAUD = 500000;

// ---------------------------------------------------------------------------
// Per-frame schedule. period_ms = how often to send. last_ms = bookkeeping.
// Data bytes are the confirmed/working presence payloads.
// ---------------------------------------------------------------------------
struct SchedFrame {
  uint32_t id;
  uint8_t  len;
  uint8_t  data[8];
  uint16_t period_ms;
  uint32_t last_ms;
  bool     hasCounter;   // if true, byte0 is replaced with an advancing counter
};

// NOTE: payloads below are the working presence values. The 0x23D entry carries
// the advancing counter (hasCounter = true) that enabled 4HI shifting.
SchedFrame sched[] = {
  // id,    len, data[8],                                             period, last, counter
  { 0x29E, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 100, 0, false },
  { 0x2A5, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 100, 0, false },
  { 0x231, 8, {0xFE,0x90,0x00,0xFE,0xFE,0xC1,0x52,0x96},  20, 0, false },
  { 0x233, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  20, 0, false },
  { 0x23D, 8, {0x00,0x00,0x00,0x00,0x00,0x47,0x09,0x0A},  20, 0, true  }, // engine data + counter
  { 0x23E, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  20, 0, false },
  { 0x551, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 100, 0, false },
  { 0x794, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 100, 0, false },
};
const size_t SCHED_N = sizeof(sched) / sizeof(sched[0]);

// ---------------------------------------------------------------------------
// 0x251 TCM neutral gear-status spoof (frozen single-frame replay).
// Sent at ~100 Hz to match the Titan TCM cadence.
// byte3 = 0x04 -> Neutral (marker-confirmed in TCM captures).
// byte0 carries an advancing counter (the TCM frame has a rolling counter).
// ---------------------------------------------------------------------------
const uint32_t TCM_ID       = 0x251;
const uint16_t TCM_PERIOD   = 10;     // ms (~100 Hz)
uint32_t       tcm_last     = 0;
uint8_t        tcm_data[8]  = {0x00,0x10,0x00,0x04,0x00,0x00,0x00,0x00}; // Neutral
uint8_t        tcm_counter  = 0;

// advancing counter for 0x23D engine data
uint8_t        eng_counter  = 0;

void sendFrame(uint32_t id, uint8_t len, const uint8_t *d) {
  CAN_message_t m;
  m.id = id;
  m.len = len;
  for (uint8_t i = 0; i < len; i++) m.buf[i] = d[i];
  Can0.write(m);
}

void setup() {
  Serial.begin(115200);
  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);
  Can0.setMaxMB(16);
  Can0.enableFIFO();
  Serial.println("Xterra<->Titan gateway: presence + 0x23D engine + 0x251 neutral spoof");
  Serial.println("Single bus @ 500k. Manual-swap application (no A/T present).");
}

void loop() {
  uint32_t now = millis();

  // scheduled presence / engine frames
  for (size_t i = 0; i < SCHED_N; i++) {
    if (now - sched[i].last_ms >= sched[i].period_ms) {
      sched[i].last_ms = now;
      uint8_t out[8];
      for (uint8_t b = 0; b < sched[i].len; b++) out[b] = sched[i].data[b];
      if (sched[i].hasCounter) {
        out[0] = eng_counter;                 // advancing counter in byte0
        eng_counter = (eng_counter + 1) & 0xFF;
      }
      sendFrame(sched[i].id, sched[i].len, out);
    }
  }

  // 0x251 TCM neutral spoof with rolling counter in byte0
  if (now - tcm_last >= TCM_PERIOD) {
    tcm_last = now;
    tcm_data[0] = tcm_counter;
    tcm_counter = (tcm_counter + 1) & 0xFF;
    sendFrame(TCM_ID, 8, tcm_data);
  }
}
