/*
 * Xterra <-> Titan Gateway  — LIVE RPM TRANSLATION   CAN1 @ 500k
 * ====================================================================
 * Full working gateway. Reads the Titan VK56DE ECM's engine RPM off the bus
 * and re-broadcasts it in the format the Xterra cluster + TCCU expect.
 *
 * TACH (confirmed):
 *   READ  Titan 0x180 bytes 0-1, BIG-endian, 0.125 RPM/LSB  -> real RPM
 *   WRITE Xterra 0x23D bytes 3-4, LITTLE-endian, RPM=raw*3.125 (raw=rpm/3.125)
 *   This drives the cluster tach AND gives the TCCU a valid engine speed
 *   (which is what made 4HI and 4LO work).
 *
 * ECT (confirmed):
 *   0x233 byte0 AND 0x23D byte7 (mirror), degC = raw-50.
 *   Currently a fixed placeholder (~90C). TODO: translate from the Titan ECM's
 *   coolant frame once that source is captured/decoded.
 *
 * Also sends: presence frames (clears U1000), 0x251 Neutral gear cycle.
 *
 * SAFETY: if no 0x180 is seen for RPM_TIMEOUT ms (ECM off/disconnected), the
 * tach falls to 0 rather than freezing at a stale value.
 *
 * SERIAL: k/m ECT +/-5C | d RPM debug print on/off
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_32> CANbus;
static const uint32_t CAN_BAUD = 500000;

// ---------- live RPM read from Titan 0x180 ----------
volatile uint16_t titanRpm = 0;
volatile uint32_t lastRpmMs = 0;
const uint32_t RPM_TIMEOUT = 500;          // ms; tach -> 0 if ECM goes quiet
bool dbg=false;

void onCanRx(const CAN_message_t &m){
  if(m.id==0x180 && m.len>=2){
    uint16_t raw=((uint16_t)m.buf[0]<<8)|m.buf[1];   // big-endian
    titanRpm=(uint16_t)(raw*0.125f);                 // 0.125 RPM/LSB
    lastRpmMs=millis();
  }
}
uint16_t currentRpm(){
  if(millis()-lastRpmMs > RPM_TIMEOUT) return 0;     // stale -> 0
  return titanRpm;
}

// ---------- ECT (placeholder ~90C until Titan coolant decoded) ----------
int ectC = 90;
uint8_t ectRaw(){ int r=ectC+50; if(r<0)r=0; if(r>255)r=255; return (uint8_t)r; }

// ---------- 0x23D b3-4 RPM encode (LE, raw=rpm/3.125) ----------
void putRpm(CAN_message_t &m,uint16_t rpm){
  uint16_t raw=(uint16_t)(rpm/3.125f + 0.5f);
  m.buf[3]=raw&0xFF; m.buf[4]=(raw>>8)&0xFF;
}

// ---------- presence frames ----------
struct TxFrame { uint32_t id; uint8_t len; uint16_t interval; uint8_t data[8]; uint32_t last; };
TxFrame presence[] = {
  {0x29E,8, 50,{0xFE,0x00,0xC0,0x00,0x00,0xFD,0x00,0x5B},0},
  {0x2A5,7, 50,{0x00,0x00,0x00,0x20,0x00,0x00,0x00},0},
  {0x231,8,100,{0xFE,0x90,0x00,0xFE,0xFE,0xC1,0x52,0x96},0},
  {0x233,8,100,{0x0A,0x00,0x00,0x18,0x00,0x00,0x00,0x00},0},  // b0 -> ECT
  {0x23E,8,100,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},0},
  {0x551,7,200,{0x00,0x00,0x00,0xA0,0x00,0x00,0x80},0},
  {0x794,3,500,{0x01,0x13,0x20},0},
};
const size_t NP=sizeof(presence)/sizeof(presence[0]);
void sendPresence(TxFrame &f,uint32_t now){
  if(now-f.last<f.interval) return; f.last=now;
  CAN_message_t m; m.id=f.id; m.len=f.len; memcpy(m.buf,f.data,f.len);
  if(f.id==0x233) m.buf[0]=ectRaw();
  CANbus.write(m);
}

// ---------- 0x23D engine data: live RPM b3-4, ECT mirror b7 ----------
uint8_t ctrSeq[4]={0x33,0x53,0x73,0x13}; uint8_t ctrIdx=0;
uint32_t last23D=0; const uint16_t I23D=25;   // ~39 Hz
void send23D(uint32_t now){
  if(now-last23D<I23D) return; last23D=now;
  CAN_message_t m; m.id=0x23D; m.len=8; for(int i=0;i<8;i++) m.buf[i]=0;
  m.buf[0]=ctrSeq[ctrIdx]; ctrIdx=(ctrIdx+1)&0x03;
  putRpm(m, currentRpm());                 // <-- LIVE translated Titan RPM
  m.buf[5]=0x47; m.buf[6]=0x09; m.buf[7]=ectRaw();   // ECT mirror
  CANbus.write(m);
}

// ---------- 0x251 TCM gear spoof (Neutral) ----------
const uint8_t NEUTRAL_CYCLE[12][8]={
  {0x10,0x00,0x00,0x04,0x00,0x6F,0x10,0x52},{0x10,0x00,0x00,0x04,0x00,0xAF,0x11,0x48},
  {0x10,0x00,0x00,0x04,0x00,0xEF,0x12,0x30},{0x10,0x00,0x00,0x04,0x00,0x2F,0x13,0x36},
  {0x10,0x00,0x00,0x04,0x00,0x6F,0x14,0x37},{0x10,0x00,0x00,0x04,0x00,0xAF,0x15,0x34},
  {0x10,0x00,0x00,0x04,0x00,0xEF,0x16,0xA8},{0x10,0x00,0x00,0x04,0x00,0x2F,0x17,0xC2},
  {0x10,0x00,0x00,0x04,0x00,0x6F,0x40,0x00},{0x10,0x00,0x00,0x04,0x00,0xAF,0x41,0x00},
  {0x10,0x00,0x00,0x04,0x00,0xEF,0x42,0x00},{0x10,0x00,0x00,0x04,0x00,0x2F,0x43,0x00},
};
uint8_t gearIdx=0; uint32_t last251=0; const uint16_t I251=10;
void send251(uint32_t now){
  if(now-last251<I251) return; last251=now;
  CAN_message_t m; m.id=0x251; m.len=8;
  for(int i=0;i<8;i++) m.buf[i]=NEUTRAL_CYCLE[gearIdx][i];
  gearIdx=(gearIdx+1)%12; CANbus.write(m);
}

void setup(){
  Serial.begin(115200); uint32_t t=millis(); while(!Serial&&millis()-t<2000){}
  CANbus.begin(); CANbus.setBaudRate(CAN_BAUD); CANbus.setMaxMB(32);
  CANbus.enableFIFO(); CANbus.enableFIFOInterrupt(); CANbus.onReceive(onCanRx);
  Serial.println("Gateway LIVE: reads Titan 0x180 RPM -> writes 0x23D b3-4 (cluster tach + TCCU).");
  Serial.println("ECT placeholder ~90C (0x233 b0 + 0x23D b7). k/m ECT, d debug.");
}
uint32_t lastStatus=0;
void loop(){
  uint32_t now=millis();
  CANbus.events();                          // service RX callback
  for(size_t i=0;i<NP;i++) sendPresence(presence[i],now);
  send23D(now); send251(now);
  if(now-lastStatus>1000){ lastStatus=now;
    if(dbg){ Serial.print("[titanRPM "); Serial.print(currentRpm());
      Serial.print(" | ECT "); Serial.print(ectC); Serial.println("C ]"); }
  }
  while(Serial.available()){
    char c=Serial.read();
    switch(c){
      case 'k': ectC+=5; Serial.print(">> ECT ");Serial.println(ectC); break;
      case 'm': ectC-=5; Serial.print(">> ECT ");Serial.println(ectC); break;
      case 'd': dbg=!dbg; Serial.println(dbg?">> debug ON":">> debug OFF"); break;
      default: break;
    }
  }
}
