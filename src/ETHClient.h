#include <ETH.h>
//extern "C" {
//	#include "freertos/FreeRTOS.h"
//	#include "freertos/timers.h"
//}

static bool eth_connected = false;
//TimerHandle_t ethReconnectTimer;

void connectToEthernet() {
  DebugPrintln("Connecting to Ethernet...");
  if(!ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE)){
    DebugPrintln("ETH Failed!");
  }else{
    DebugPrintln("ETH Connected!");
    //xTimerStop(ethReconnectTimer, 0);
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      DebugPrintln("ETH Started");
      //set eth hostname here
      ETH.setHostname(DEVICE_ID);
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      DebugPrintln("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      DebugPrint("ETH MAC: ");
      DebugPrint(ETH.macAddress());
      DebugPrint(", IPv4: ");
      DebugPrint(ETH.localIP());
      if (ETH.fullDuplex()) {
        DebugPrint(", FULL_DUPLEX");
      }
      DebugPrint(", ");
      DebugPrint(ETH.linkSpeed());
      DebugPrintln("Mbps");
      eth_connected = true;
      //connectToMqtt();
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      DebugPrintln("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      DebugPrintln("ETH Stopped");
      eth_connected = false;   
      //xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi   
      //xTimerStart(ethReconnectTimer, 0);
      break;
    default:    
      DebugPrint("Wifi event: ");
      DebugPrintln(event);
      break;
  }
}



void ETHSetup(){
  //NOT required for WESP32 will be for WT32-ETH01
  //pinMode(ETH_RESET_P, OUTPUT);
  //digitalWrite(ETH_RESET_P, HIGH);   // GPIO16 = Force Hi
  //delay(100);

  DebugPrintln("wifi event handler added");
  WiFi.onEvent(WiFiEvent);
    
  DebugPrintln("ETH Begin");
  connectToEthernet();

    // Setup timers for mqtt and ethernet reconnection
  //mqttReconnectTimer =  xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  //ethReconnectTimer = xTimerCreate("ethTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToEthernet));

}


void ETHLoop(){
  if (eth_connected) {
    //testClient("google.com", 80);
  }else{    
    DebugPrintln("eth not connected!");
  }
  //delay(5000);
}

