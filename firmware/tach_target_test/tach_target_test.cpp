/*
 * Tach Target Test   Teensy 4.1 + FlexCAN_T4   CAN1 @ 500k
 * ==========================================================================
 * Tool for finding which 0x23x frame/byte/scale the Xterra cluster and TCCU
 * read for ENGINE SPEED. Injects a controllable RPM value onto candidate
 * frames so you can watch the cluster tach and the TCCU's ENGINE SPEED monitor
 * respond.
 *
 * STATUS: target frame/scale NOT yet confirmed. The TCCU currently reads a
 * frozen/implausible engine speed, indicating the injected encoding is not
 * what it decodes. Pending a stock-VQ40 (Frontier) rev-sweep capture to
 * confirm the real engine-speed frame, byte position, endianness, and scale.
 * See docs/SIGNALS.md.
 *
 * CANDIDATE TARGETS (toggle with keys): 0x23D, 0x1F9, 0x580
 * Default encoding: RPM raw = rpm / scale, placed little-endian in bytes 3-4.
 *   (scale default 0.125 RPM/LSB, the common Nissan convention)
 *
 * SERIAL CONTROLS (char + Enter):
 *   1 : target 0x23D     2 : target 0x1F9     3 : target 0x580
 *   w : sweep ON (idle->3000->idle ramp)   x : sweep OFF (hold fixed)
 *   u / j : fixed RPM up / down (when sweep off), +-100
 *   + / - : scale up / down (RPM per LSB)
 *   s : status
 *
 * Use ONE target at a time so you can attribute any tach/monitor movement.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
static const uint32_t CAN_BAUD = 500000;

uint32_t targetId  = 0x23D;
float    scale     = 0.125f;     // RPM per LSB
bool     sweep     = false;
float    fixedRpm  = 800.0f;
uint16_t period_ms = 20;
uint32_t last_ms   = 0;

// sweep state
float    sweepRpm  = 800.0f;
float    sweepDir  = 1.0f;
const float SWEEP_MIN = 700.0f, SWEEP_MAX = 3000.0f, SWEEP_STEP = 40.0f;

uint8_t counter = 0;

void sendTach() {
  float rpm = sweep ? sweepRpm : fixedRpm;
  uint16_t raw = (uint16_t)(rpm / scale);

  CAN_message_t m;
  m.id = targetId; m.len = 8;
  for (uint8_t i = 0; i < 8; i++) m.buf[i] = 0x00;
  m.buf[0] = counter++;            // advancing counter (some frames need it)
  m.buf[3] = raw & 0xFF;           // RPM low byte  (little-endian)
  m.buf[4] = (raw >> 8) & 0xFF;    // RPM high byte
  Can0.write(m);

  if (sweep) {
    sweepRpm += sweepDir * SWEEP_STEP;
    if (sweepRpm >= SWEEP_MAX) { sweepRpm = SWEEP_MAX; sweepDir = -1.0f; }
    if (sweepRpm <= SWEEP_MIN) { sweepRpm = SWEEP_MIN; sweepDir =  1.0f; }
  }
}

void status() {
  Serial.print("target=0x"); Serial.print(targetId, HEX);
  Serial.print(" scale="); Serial.print(scale, 4);
  Serial.print(" sweep="); Serial.print(sweep ? "ON" : "OFF");
  Serial.print(" fixedRpm="); Serial.println(fixedRpm, 0);
}

void setup() {
  Serial.begin(115200);
  Can0.begin(); Can0.setBaudRate(CAN_BAUD); Can0.setMaxMB(16); Can0.enableFIFO();
  Serial.println("Tach target test. 1/2/3 target, w/x sweep, u/j rpm, +/- scale, s status.");
  status();
}

void loop() {
  uint32_t now = millis();
  if (now - last_ms >= period_ms) { last_ms = now; sendTach(); }

  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '1': targetId = 0x23D; status(); break;
      case '2': targetId = 0x1F9; status(); break;
      case '3': targetId = 0x580; status(); break;
      case 'w': sweep = true;  status(); break;
      case 'x': sweep = false; status(); break;
      case 'u': fixedRpm += 100; status(); break;
      case 'j': if (fixedRpm > 100) fixedRpm -= 100; status(); break;
      case '+': scale += 0.025f; status(); break;
      case '-': if (scale > 0.05f) scale -= 0.025f; status(); break;
      case 's': status(); break;
      default: break;
    }
  }
}
