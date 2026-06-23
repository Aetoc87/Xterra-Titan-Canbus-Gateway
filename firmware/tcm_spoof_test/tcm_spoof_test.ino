/*
 * TCM Spoof Test   0x251 gear-status   Teensy 4.1 + FlexCAN_T4   CAN1 @ 500k
 * ==========================================================================
 * Bench/vehicle test tool for the Titan TCM gear-status frame (0x251).
 * Used to find what the ECM needs to clear the lost-TCM comms code on a
 * manual swap (no automatic TCM present).
 *
 * *** SAFETY: appropriate ONLY because no automatic transmission is present.
 *     This defeats a TCM-coordination check. Do NOT use with an A/T. ***
 *
 * RESULT TO DATE: a frozen neutral replay (Mode B) CLEARS the U1000/U1001
 * lost-TCM comms code, but does NOT restore full throttle. The remaining
 * ~25% throttle/torque limit is tune-level (UpRev/Osiris), not comms.
 *
 * ---------------------------------------------------------------------------
 * 0x251 DECODE (from Titan parts-truck captures)
 *   DLC 8, ~100 Hz (10 ms)
 *   byte3 = gear bitfield: 0x01=Park, 0x02=Reverse, 0x04=Neutral, 0x08=Drive
 *           (Park and Neutral marker-confirmed; byte1 also =0x10 in Drive)
 *   byte0 / byte5 = rolling counter
 *   byte6 = multiplex index, byte7 = multiplexed payload (NOT a live checksum)
 *
 * SERIAL CONTROLS (type char + Enter):
 *   a / b / c : select Mode A / B / C
 *   n         : gear = Neutral (byte3 = 0x04)
 *   p         : gear = Park    (byte3 = 0x01)
 *   + / -     : increase / decrease send rate
 *   s         : print current status
 *
 *   Mode A: static frame, no counter movement (byte0 fixed)
 *   Mode B: advancing rolling counter in byte0 (frozen-payload replay)  <-- clears code
 *   Mode C: advancing counter + cycling mux index in byte6
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
static const uint32_t CAN_BAUD = 500000;

const uint32_t TCM_ID = 0x251;
uint16_t period_ms = 10;
uint32_t last_ms   = 0;

char    mode    = 'b';
uint8_t counter = 0;
uint8_t muxIdx  = 0;
uint8_t gearByte3 = 0x04;   // default Neutral

uint8_t base[8] = {0x00,0x10,0x00,0x04,0x00,0x00,0x00,0x00};

void sendTCM() {
  CAN_message_t m;
  m.id = TCM_ID; m.len = 8;
  for (uint8_t i = 0; i < 8; i++) m.buf[i] = base[i];
  m.buf[3] = gearByte3;

  switch (mode) {
    case 'a':                       // static
      break;
    case 'b':                       // rolling counter
      m.buf[0] = counter;
      m.buf[5] = counter;
      counter++;
      break;
    case 'c':                       // counter + cycling mux
      m.buf[0] = counter;
      m.buf[5] = counter;
      counter++;
      m.buf[6] = muxIdx;
      muxIdx = (muxIdx + 1) & 0x07;
      break;
  }
  Can0.write(m);
}

void status() {
  Serial.print("mode="); Serial.print(mode);
  Serial.print(" gear(byte3)=0x"); Serial.print(gearByte3, HEX);
  Serial.print(" rate="); Serial.print(period_ms); Serial.println("ms");
}

void setup() {
  Serial.begin(115200);
  Can0.begin(); Can0.setBaudRate(CAN_BAUD); Can0.setMaxMB(16); Can0.enableFIFO();
  Serial.println("TCM spoof test 0x251. a/b/c mode, n/p gear, +/- rate, s status.");
  status();
}

void loop() {
  uint32_t now = millis();
  if (now - last_ms >= period_ms) { last_ms = now; sendTCM(); }

  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'a': case 'b': case 'c': mode = c; status(); break;
      case 'n': gearByte3 = 0x04; status(); break;   // Neutral
      case 'p': gearByte3 = 0x01; status(); break;   // Park
      case '+': if (period_ms > 2) period_ms--; status(); break;
      case '-': if (period_ms < 200) period_ms++; status(); break;
      case 's': status(); break;
      default: break;
    }
  }
}
