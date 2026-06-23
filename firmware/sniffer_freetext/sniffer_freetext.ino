/*
 * CAN Sniffer with FREE-TEXT markers   Teensy 4.1 + FlexCAN_T4   CAN1 @ 500k
 * ==========================================================================
 * Listen-only logger for the 2006 Frontier (VQ40) capture session.
 *
 * MARKERS: type any text + Enter and it stamps a full marker line into the log,
 * e.g. you type:   T=Cold  <Enter>
 * and the log gets:  123456 ms  # MARKER: T=Cold
 *
 * The firmware buffers your keystrokes until Enter, so it no longer matters
 * whether the terminal sends chars one at a time — nothing is stamped until you
 * press Enter. Backspace is handled so typos can be corrected.
 *
 * Suggested markers for this session (type whatever is meaningful):
 *   KOEO            (key on engine off, before start)
 *   IDLE
 *   REV START / REV END
 *   T=Cold 20C  /  T=50C  /  T=Warm 90C     (note temps during warm-up)
 *   BRAKE PRESS / BRAKE RELEASE
 *   MOVING / STOPPED
 *
 * LISTEN-ONLY: transmits nothing to the truck. Safe on someone else's vehicle.
 * Capture to file:  pio device monitor > frontier_<name>.txt
 * Tap: OBD-II pin 6 (CAN-H) / pin 14 (CAN-L), 500 kbit/s.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_512, TX_SIZE_16> Can0;
static const uint32_t CAN_BAUD = 500000;

// ---- marker input buffer ----
char markerBuf[120];
uint8_t markerLen = 0;

void printFrame(const CAN_message_t &msg) {
  Serial.print(millis());
  Serial.print(" ms  ID:0x");
  Serial.print(msg.id, HEX);
  Serial.print("  LEN:");
  Serial.print(msg.len);
  Serial.print("  DATA: ");
  for (uint8_t i = 0; i < msg.len; i++) {
    if (msg.buf[i] < 0x10) Serial.print('0');
    Serial.print(msg.buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void canSniff(const CAN_message_t &msg) { printFrame(msg); }

void stampMarker(const char *text) {
  uint32_t t = millis();
  Serial.println();
  Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  Serial.print(  ">>>>>>  MARKER: ");
  Serial.print(text);
  Serial.print("   @ "); Serial.print(t); Serial.println(" ms");
  // machine-readable line the parser keys on:
  Serial.print(t); Serial.print(" ms  # MARKER: "); Serial.println(text);
  Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}
  Can0.begin();
  Can0.setBaudRate(CAN_BAUD);
  Can0.setMaxMB(16);
  Can0.enableFIFO();
  Can0.enableFIFOInterrupt();
  Can0.onReceive(canSniff);

  Serial.println();
  Serial.println("#####################################################");
  Serial.println(">>> LISTEN-ONLY SNIFFER (free-text markers)        <<<");
  Serial.println(">>> Type any text + Enter to stamp a marker.       <<<");
  Serial.println(">>> e.g.  T=Cold  <Enter>  ->  # MARKER: T=Cold    <<<");
  Serial.println(">>> Transmits NOTHING to the vehicle.              <<<");
  Serial.println("#####################################################");
}

void loop() {
  Can0.events();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (markerLen > 0) {
        markerBuf[markerLen] = '\0';
        stampMarker(markerBuf);
        markerLen = 0;
      }
      // ignore stray \n after \r (or empty Enter)
    } else if (c == 0x08 || c == 0x7F) {       // backspace / delete
      if (markerLen > 0) markerLen--;
    } else if (markerLen < sizeof(markerBuf) - 1) {
      markerBuf[markerLen++] = c;              // buffer, don't stamp yet
    }
  }
}
