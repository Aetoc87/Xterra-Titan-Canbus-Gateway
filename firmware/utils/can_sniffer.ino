#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> CANbus;

struct Stat { uint32_t id; uint32_t count; uint32_t last_seen; };
Stat stats[256]; int used=0;
int find(uint32_t id){ for(int i=0;i<used;i++) if(stats[i].id==id) return i; if(used<256){ stats[used].id=id; stats[used].count=0; stats[used].last_seen=0; return used++; } return -1; }

void setup(){ 
  Serial.begin(115200); 
  while(!Serial){} 
  CANbus.begin(); 
  CANbus.setBaudRate(500000); 
  Serial.println("Teensy CAN sniffer ready (CAN1)"); 
}

void loop(){
  CAN_message_t m;
  if(CANbus.read(m)){
    int i=find(m.id);
    if(i>=0){ stats[i].count++; stats[i].last_seen=millis(); }
    Serial.printf("%lu ms ID:0x%03X LEN:%d DATA:", millis(), m.id, m.len);
    for(int j=0;j<m.len;j++) Serial.printf(" %02X", m.buf[j]);
    Serial.println();
  }

  static uint32_t last=0;
  if(millis()-last>1000){
    last=millis();
    Serial.println("ID  Count/s  LastSeen(ms ago)");
    for(int k=0;k<used;k++){
      uint32_t ago = (millis() - stats[k].last_seen);
      Serial.printf("0x%03X  %lu  %lu\n", stats[k].id, stats[k].count, ago);
      stats[k].count = 0;
    }
    Serial.println();
  }
}
