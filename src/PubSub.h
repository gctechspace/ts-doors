#include <WiFiClient.h>
#include <PubSubClient.h>

void subcallback(char* topic, byte* payload, unsigned int length) {
  DebugPrint("Message arrived [");
  DebugPrint(topic);
  DebugPrint("] ");
  for (int i=0;i<length;i++) {
    DebugPrint((char)payload[i]);
  }
  DebugPrintln();

  //copy payload byte arrary to char array
  char payloads[sizeof( payload ) + sizeof( ( char )'\0' )] = { '\0' };
  memcpy( payloads, payload, length );
  onmessagereceived(topic, payloads);
}

WiFiClient psClient;
//EthernetClient psClient;
PubSubClient client(psClient);

void mqttsubscribe(const char* topic, int qos){
      client.subscribe(topic);
}

//generic message handler
void messagesend(const char* topic, const char* payload, int qos, bool retain){
  client.publish(topic, payload);
}

void messagesend(const char* topic, const char* payload){
  messagesend(topic, payload,0,false);
}

void onMqttConnect(bool sessionPresent)
{
  messagesend(MQTT_LOG_TOPIC, "{ \"available\": 1 }" ,0, 1); // Retain the state topic

  mqttsubscribe(MQTT_RESTART_TOPIC, 0);
  mqttsubscribe(MQTT_LOCK_TOPIC, 1);

  DebugPrintln("[INFO]: mqtt, Connected as " DEVICE_ID);
  DebugPrintln("[INFO]: mqtt, Subscribed: " MQTT_RESTART_TOPIC);
  DebugPrintln("[INFO]: mqtt, Subscribed: " MQTT_LOCK_TOPIC);

  if (!client.connected()) {
    DebugPrintln("Client is not actually connected!");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DebugPrint("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(DEVICE_ID)) {
      DebugPrint("connected with version: ");
      DebugPrintln(VERSION);
      onMqttConnect(1);    
    } else {
      DebugPrint("failed, rc=");
      DebugPrint(client.state());
      DebugPrintln(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void pubsubsetup()
{
 
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(subcallback);

}

void pubsubloop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

