#define VERSION "0.9"
#include <Arduino.h>

#define DEBUG 1
#define DEBUG_OI Serial
#define UseEthernet 1

#include "config.h"

#define MQTT_RFID_TOPIC DEVICE_TYPE "/" DEVICE_ID "/rfid"
#define MQTT_LOCK_TOPIC DEVICE_TYPE "/" DEVICE_ID "/lock"
#define MQTT_STATE_TOPIC DEVICE_TYPE "/" DEVICE_ID "/state"
#define MQTT_LOG_TOPIC DEVICE_TYPE "/" DEVICE_ID "/log"
#define MQTT_RESTART_TOPIC DEVICE_TYPE "/" DEVICE_ID "/restart"

#include "utils.h"
#include "debug.h"

bool triprelay = false;

//message validation
#include "interlock.h"

void onmessagereceived(const char *topic, char *payload){
 if(validmessage(topic,payload)==true){
  triprelay = true;
 }
}

#include "ETHClient.h"
#include "PubSub.h"
#include "rfid.h"
#include "relay.h"
#include "ota.h"


void setup() {

  DebugBegin(115200);
  
  DebugPrintln("Setup started");

  interlocksetup();
  relaysetup();
  RFIDSetup();  
  ETHSetup();
  pubsubsetup();
  otasetup();
  
  DebugPrintln("Setup complete");

}


void loop() {

  ETHLoop();
  relayloop();
  RFIDLoop();
  pubsubloop();
  otaloop();
  delay(50);

}


