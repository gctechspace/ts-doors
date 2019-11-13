#include <WiFi.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}
#include <AsyncMqttClient.h>
#include <MFRC522.h>
#include <SPI.h>
#include <ErriezCRC32.h>

#include "config.h"
#include "utils.h"

#define MQTT_RFID_TOPIC "doors/" DEVICE_ID "/rfid"
#define MQTT_LOCK_TOPIC "doors/" DEVICE_ID "/lock"

TimerHandle_t wifiReconnectTimer;
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

enum LockResponse {
  LOCK_RESPONSE_NEWCARD = 0,
  LOCK_RESPONSE_NOACCESS,
  LOCK_RESPONSE_UNPAID,
  LOCK_RESPONSE_GRANTED,
  MAX_LOCK_RESPONSES
};

uint32_t lockTopicHash;
uint32_t lockResponseHashes[MAX_LOCK_RESPONSES];

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");

  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    connectToMqtt();

    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");

    xTimerStop(mqttReconnectTimer, 0);
    xTimerStart(wifiReconnectTimer, 0);

    break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");

  // Subscribe to the lock response messages
  mqttClient.subscribe(MQTT_LOCK_TOPIC, 1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("MQTT disconnected.");

  // Mqtt was disconnected and wifi is connected, try to reconnect to mqtt
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttMessage(char *topic, char *payload,
                   AsyncMqttClientMessageProperties properties, size_t len,
                   size_t index, size_t total) {
  // Calculate the crc for the topic
  uint32_t topicHash = crc32String(topic);

  // Ensure the message came from our lock topic
  if (topicHash == lockTopicHash) {
    // Calculate the crc for the payload
    uint32_t payloadHash = crc32Buffer(payload, len);

    // Check payload hash against known response hashes
    if (payloadHash == lockResponseHashes[LOCK_RESPONSE_NEWCARD]) {
      Serial.println("NEWCARD");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_NOACCESS]) {
      Serial.println("NOACCESS");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_UNPAID]) {
      Serial.println("UNPAID");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_GRANTED]) {
      Serial.println("GRANTED");

      // Set the door state
      doorState = STATE_OPENING;
    } else {
      Serial.println("Unknown payload");
    }
  } else {
    Serial.println("Unknown topic");
  }
}

void setup() {
  // Initialize Serial, SPI and RFID
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  // Initialize crcs
  lockTopicHash = crc32String(MQTT_LOCK_TOPIC);
  lockResponseHashes[LOCK_RESPONSE_NEWCARD] = crc32String("NEWCARD");
  lockResponseHashes[LOCK_RESPONSE_NOACCESS] = crc32String("NOACCESS");
  lockResponseHashes[LOCK_RESPONSE_UNPAID] = crc32String("UNPAID");
  lockResponseHashes[LOCK_RESPONSE_GRANTED] = crc32String("GRANTED");

  // Setup timers for mqtt and wifi reconnection
  mqttReconnectTimer =
      xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                   reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer =
      xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                   reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  // Bind to the wifi events
  WiFi.onEvent(WiFiEvent);

  // Initialize the mqtt client
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // Connect to the wifi
  connectToWifi();
}

void loop() {
  // Wait for new card scan and read it
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Make sure card is a mifare card
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));

    return;
  }

  // Convert binary rfid to hex string
  char rfidStr[64] = {0};
  hexToAscii(rfid.uid.uidByte, rfidStr, rfid.uid.size);

  // Send rfid via mqtt
  mqttClient.publish(MQTT_RFID_TOPIC, 1, false, rfidStr);

  // Halt picc
  rfid.PICC_HaltA();

  // Stop encryption on pcd
  rfid.PCD_StopCrypto1();
}
