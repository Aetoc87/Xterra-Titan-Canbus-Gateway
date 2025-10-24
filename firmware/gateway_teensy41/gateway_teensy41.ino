#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> chassisCAN;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> powerCAN;

#define CHASSIS_BAUD 500000
#define POWERTRAIN_BAUD 500000

volatile uint16_t rpm_chassis = 0;
volatile bool mil_on = false;

void setup() {
  Serial.begin(115200);
  chassisCAN.begin();
  powerCAN.begin();
  chassisCAN.setBaudRate(CHASSIS_BAUD);
  powerCAN.setBaudRate(POWERTRAIN_BAUD);
  Serial.println("Teensy 4.1 gateway online");
}

void loop() {
  CAN_message_t msg;

  if (powerCAN.read(msg)) {
    // Example placeholder: detect Titan ECM RPM
  }

  if (chassisCAN.read(msg)) {
    // Example placeholder
  }

  static uint32_t lastSynth = 0;
  if (millis() - lastSynth > 50) {
    lastSynth = millis();
    CAN_message_t out;
    out.id = 0x180;
    out.len = 8;
    memset(out.buf, 0, 8);
    uint16_t val = rpm_chassis / 4;
    out.buf[2] = (val >> 8) & 0xFF;
    out.buf[3] = val & 0xFF;
    chassisCAN.write(out);
  }
}
