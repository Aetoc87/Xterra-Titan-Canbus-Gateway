/*
 * Frontier Rev-Sweep REPLAY -- G1 NARROWING  (Teensy 4.x)  CAN1 @ 500k
 * ====================================================================
 * The tach signal was bisected to G1 = { 0x1F9 0x231 0x233 0x23D 0x23E 0x253 }.
 * Single-frame injection of 0x1F9/0x231/0x23D/0x253 did NOT drive the tach, but
 * all six together DO -> the tach likely needs a COMBINATION (an RPM frame plus
 * an enable/valid flag). So we narrow by ELIMINATION: play all six, then turn
 * ONE off at a time and see if the tach dies.
 *
 * Only G1 is replayed here (other groups are dropped to keep the bus quiet-ish;
 * 4x4/VDC lights may flicker -- ignore, we only watch the TACH).
 *
 * Each of the six IDs has its own toggle:
 *   q : 0x1F9     w : 0x231     e : 0x233
 *   r : 0x23D     t : 0x23E     y : 0x253
 *   a : all six ON          ?  : status
 *   space/p pause           R  : restart sweep
 *
 * METHOD:
 *   1. 'a' all on -> confirm tach sweeps.
 *   2. turn ONE off (e.g. 'q'). Tach still sweep? that ID is not needed -> leave off or back on.
 *      Tach DIES? that ID is NECESSARY -> turn back on, note it.
 *   3. Repeat for each. You may find ONE carrier, or TWO (carrier + enabler).
 *   4. Final check: solo the necessary one/two (all others off) -> tach should sweep.
 */

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include "replay_data.h"

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_32> CANbus;
static const uint32_t CAN_BAUD = 500000;

uint16_t idx = 0;
uint32_t nextDue = 0;
bool paused = false;

// six G1 ids and their enable flags
const uint16_t G1[6] = {0x1F9,0x231,0x233,0x23D,0x23E,0x253};
bool en[6] = {true,true,true,true,true,true};

int g1index(uint16_t id){
  for(int i=0;i<6;i++) if(G1[i]==id) return i;
  return -1;   // not in G1 -> never sent
}

void printEn(){
  Serial.print(">> ");
  const char* nm[6]={"1F9","231","233","23D","23E","253"};
  for(int i=0;i<6;i++){ Serial.print(nm[i]); Serial.print(en[i]?":on ":":OFF "); }
  Serial.println();
}

void setup(){
  Serial.begin(115200);
  uint32_t t=millis(); while(!Serial && millis()-t<2000){}
  CANbus.begin(); CANbus.setBaudRate(CAN_BAUD); CANbus.setMaxMB(32);
  Serial.println("REPLAY G1 narrowing. ECM disconnected, this sketch only.");
  Serial.println("q=1F9 w=231 e=233 r=23D t=23E y=253 | a=all R=restart space=pause ?=status");
  printEn();
  nextDue = millis();
}

void loop(){
  uint32_t now = millis();
  if(!paused && (int32_t)(now - nextDue) >= 0){
    const RFrame &fr = REPLAY[idx];
    int gi = g1index(fr.id);
    if(gi>=0 && en[gi]){                 // only G1 ids, only if enabled
      CAN_message_t m; m.id=fr.id; m.len=fr.dlc;
      for(uint8_t i=0;i<fr.dlc && i<8;i++) m.buf[i]=fr.data[i];
      CANbus.write(m);
    }
    static uint32_t lastPrint=0;
    if(fr.id==0x1F9 && now-lastPrint>500){
      lastPrint=now; uint16_t raw=(fr.data[2]<<8)|fr.data[3];
      Serial.print("replay RPM ~ "); Serial.println(raw*0.125,0);
    }
    idx++; if(idx>=REPLAY_N) idx=0;
    nextDue = now + REPLAY[idx].dt;
  }
  while(Serial.available()){
    char c=Serial.read();
    switch(c){
      case 'q': en[0]=!en[0]; printEn(); break;
      case 'w': en[1]=!en[1]; printEn(); break;
      case 'e': en[2]=!en[2]; printEn(); break;
      case 'r': en[3]=!en[3]; printEn(); break;
      case 't': en[4]=!en[4]; printEn(); break;
      case 'y': en[5]=!en[5]; printEn(); break;
      case 'a': for(int i=0;i<6;i++) en[i]=true; Serial.println(">> ALL ON"); printEn(); break;
      case 'R': idx=0; nextDue=now; Serial.println(">> RESTART"); break;
      case ' ': case 'p': paused=!paused; Serial.println(paused?">> PAUSED":">> RESUME"); break;
      case '?': printEn(); break;
      default: break;
    }
  }
}
