/*
 * Xterra <-> Titan Gateway — LIVE RPM + LIVE ECT (production build)  CAN1 @ 500k
 * ==========================================================================
 * Reads the Titan VK56DE ECM's engine RPM and coolant temp off the bus and
 * re-broadcasts them in the format the Xterra cluster + TCCU expect.
 *
 * TACH (confirmed):
 *   READ  Titan 0x180 b0-1 BIG-endian x0.125 -> RPM
 *   WRITE Xterra 0x23D b3-4 LITTLE-endian, raw = rpm/3.125
 *   Drives the cluster tach AND gives the TCCU valid engine speed (enables
 *   4HI/4LO). The 3.125 scale is essential; 0.125 is 25x too large.
 *
 * ECT (confirmed):
 *   READ  Titan 0x551 byte0 (DLC 8 ONLY), Titan encoding degC = raw - 40
 *   WRITE Xterra 0x233 b0 AND 0x23D b7 (mirror), Xterra encoding degC = raw - 50
 *   => xterra_raw = titan_raw + 10.  Both Xterra bytes must agree.
 *
 * 0x551 — IMPORTANT, do NOT "optimize away":
 *   The gateway sends its own DLC-7 0x551 presence frame. The VDC/ABS system
 *   reads something from it; removing it triggers a slip light and blocks
 *   steering-angle-sensor reset. The Titan ECM ALSO sends 0x551 (DLC 8) with
 *   coolant in b0 — the RX path reads coolant ONLY from the DLC-8 version so
 *   the gateway ignores its own DLC-7 echo (no feedback loop).
 *
 * NOT included (deliberately):
 *   - 0x251 TCM neutral spoof: removed — it did NOT clear U0101 in practice
 *     (confirmed via DTC read with spoof active). The ECM's remaining issues
 *     (U0101, torque limit) trace to the missing TCM and are addressed at the
 *     tune level (UpRev), not by this frame.
 *   - Vehicle-speed injection: the chassis ABS speed path (ECM "vehicle speed
 *     sensor" PID) works from the real 2005 frames while driving. The only dead
 *     speed path is the TCM-sourced one (ECM "VEHICLE SPEED" PID = 0), which no
 *     chassis frame feeds — that's part of the TCM/tune question, not a gateway
 *     translation. See docs/MASTER_NOTES.md.
 *
 * Stale-data safety: no 0x180 for 500 ms -> tach 0. No Titan 0x551 for 2 s ->
 * ECT falls back to ~90 C placeholder.
 *
 * SERIAL: d = debug print on/off
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_32> CANbus;
static const uint32_t CAN_BAUD = 500000;

// ---------- live RPM from Titan 0x180 ----------
volatile uint16_t titanRpm = 0;
volatile uint32_t lastRpmMs = 0;
const uint32_t RPM_TIMEOUT = 500;

// ---------- live ECT from Titan 0x551 (DLC 8 only) ----------
volatile uint8_t  titanEctRaw = 0;
volatile uint32_t lastEctMs = 0;
const uint32_t ECT_TIMEOUT = 2000;
const uint8_t   ECT_FALLBACK_XRAW = 140;   // ~90 C on Xterra scale if signal lost

bool dbg = false;

void onCanRx(const CAN_message_t &m){
  if(m.id==0x180 && m.len>=2){
    uint16_t raw=((uint16_t)m.buf[0]<<8)|m.buf[1];   // big-endian
    titanRpm=(uint16_t)(raw*0.125f);
    lastRpmMs=millis();
  } else if(m.id==0x551 && m.len==8){                // Titan's DLC-8 only
    titanEctRaw=m.buf[0];                            // ignore our own DLC-7 echo
    lastEctMs=millis();
  }
}
uint16_t currentRpm(){ return (millis()-lastRpmMs>RPM_TIMEOUT)?0:titanRpm; }
uint8_t currentEctXraw(){
  if(millis()-lastEctMs > ECT_TIMEOUT) return ECT_FALLBACK_XRAW;
  int x=(int)titanEctRaw+10; if(x<0)x=0; if(x>255)x=255;
  return (uint8_t)x;
}

// ---------- presence frames ----------
struct TxFrame { uint32_t id; uint8_t len; uint16_t interval; uint8_t data[8]; uint32_t last; };
TxFrame presence[] = {
  {0x29E,8, 50,{0xFE,0x00,0xC0,0x00,0x00,0xFD,0x00,0x5B},0},
  {0x2A5,7, 50,{0x00,0x00,0x00,0x20,0x00,0x00,0x00},0},
  {0x231,8,100,{0xFE,0x90,0x00,0xFE,0xFE,0xC1,0x52,0x96},0},
  {0x233,8,100,{0x0A,0x00,0x00,0x18,0x00,0x00,0x00,0x00},0},  // b0 -> live ECT
  {0x23E,8,100,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},0},
  {0x551,7,200,{0x00,0x00,0x00,0xA0,0x00,0x00,0x80},0},        // DLC 7 — VDC needs it; b0 -> live ECT
  {0x794,3,500,{0x01,0x13,0x20},0},
};
const size_t NP=sizeof(presence)/sizeof(presence[0]);
void sendPresence(TxFrame &f,uint32_t now){
  if(now-f.last<f.interval) return; f.last=now;
  CAN_message_t m; m.id=f.id; m.len=f.len; memcpy(m.buf,f.data,f.len);
  if(f.id==0x233) m.buf[0]=currentEctXraw();
  if(f.id==0x551) m.buf[0]=currentEctXraw();
  CANbus.write(m);
}

// ---------- 0x23D engine data: live RPM b3-4, live ECT mirror b7 ----------
uint8_t ctrSeq[4]={0x33,0x53,0x73,0x13}; uint8_t ctrIdx=0;
uint32_t last23D=0; const uint16_t I23D=25;   // ~39 Hz
void send23D(uint32_t now){
  if(now-last23D<I23D) return; last23D=now;
  CAN_message_t m; m.id=0x23D; m.len=8; for(int i=0;i<8;i++) m.buf[i]=0;
  m.buf[0]=ctrSeq[ctrIdx]; ctrIdx=(ctrIdx+1)&0x03;
  uint16_t r=(uint16_t)(currentRpm()/3.125f + 0.5f);   // correct 3.125 scale
  m.buf[3]=r&0xFF; m.buf[4]=(r>>8)&0xFF;
  m.buf[5]=0x47; m.buf[6]=0x09; m.buf[7]=currentEctXraw();
  CANbus.write(m);
}

void setup(){
  Serial.begin(115200); uint32_t t=millis(); while(!Serial&&millis()-t<2000){}
  CANbus.begin(); CANbus.setBaudRate(CAN_BAUD); CANbus.setMaxMB(32);
  CANbus.enableFIFO(); CANbus.enableFIFOInterrupt(); CANbus.onReceive(onCanRx);
  Serial.println("Gateway: RPM 0x180->0x23D (3.125), ECT 0x551->0x233/0x23D (+10). No spoof/inject.");
  Serial.println("d = debug");
}
uint32_t lastStatus=0;
void loop(){
  uint32_t now=millis();
  CANbus.events();
  for(size_t i=0;i<NP;i++) sendPresence(presence[i],now);
  send23D(now);
  if(now-lastStatus>1000){ lastStatus=now;
    if(dbg){
      Serial.print("[RPM "); Serial.print(currentRpm());
      Serial.print(" | ECT->cluster "); Serial.print((int)currentEctXraw()-50);
      Serial.println("C ]");
    }
  }
  while(Serial.available()){
    char c=Serial.read();
    if(c=='d'){ dbg=!dbg; Serial.println(dbg?">> debug ON":">> debug OFF"); }
  }
}
