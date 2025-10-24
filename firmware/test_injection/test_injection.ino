#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> CANbus;

struct TxFrame { uint32_t id; uint8_t len; uint8_t data[8]; uint16_t period; uint32_t last; };
TxFrame frames[] = {
  {0x180,8,{0x00,0x00,0x5D,0xC4,0x3F,0x00,0x11,0x11},50,0},
  {0x29E,8,{0x00,0x00,0xC0,0x00,0x00,0xFD,0x02,0x5F},100,0},
  {0x160,7,{0x43,0xF5,0xDC,0x00,0x08,0xFF,0xC0,0x00},50,0},
  {0x6F7,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03},200,0},
};

void setup(){
  Serial.begin(115200);
  while(!Serial) {}
  CANbus.begin();
  CANbus.setBaudRate(500000);
  Serial.println("Teensy injector ready on CAN1");
}

void loop(){
  uint32_t now = millis();
  for(auto &f: frames){
    if(now - f.last >= f.period){
      f.last = now;
      CAN_message_t m;
      m.id = f.id; m.len = f.len; memcpy(m.buf, f.data, 8);
      CANbus.write(m);
      Serial.printf("TX 0x%03X\n", f.id);
    }
  }
}
