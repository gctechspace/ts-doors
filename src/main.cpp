#include <ETH.h>
#include <SPI.h>

#include <AsyncMqttClient.h>
#include <ErriezCRC32.h>
#include <MFRC522.h>

#include "config.h"
#include "utils.h"

#define MQTT_RFID_TOPIC "doors/" DEVICE_ID "/rfid"
#define MQTT_LOCK_TOPIC "doors/" DEVICE_ID "/lock"
#define MQTT_STATE_TOPIC "doors/" DEVICE_ID "/state"

TimerHandle_t ethReconnectTimer;
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

enum DoorState { DOOR_CLOSED = 0, DOOR_WAITING, DOOR_OPENED, MAX_DOOR_STATES };

DoorState doorState = DOOR_CLOSED;
uint32_t doorOpenTime = 0;

bool doorChangedState = true;
uint32_t doorChangedTime = 0;

enum LockResponse {
  LOCK_RESPONSE_NEWCARD = 0,
  LOCK_RESPONSE_NOACCESS,
  LOCK_RESPONSE_UNPAID,
  LOCK_RESPONSE_GRANTED,
  MAX_LOCK_RESPONSES
};

uint32_t lockTopicHash;
uint32_t lockResponseHashes[MAX_LOCK_RESPONSES];

bool ethernetConnected = false;

void connectToEthernet() {
  Serial.println("Connecting to Ethernet...");

  ETH.begin();
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");

  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_ETH_START:
    Serial.println("Ethernet Started");

    ETH.setHostname(DEVICE_ID);

    break;

  case SYSTEM_EVENT_ETH_CONNECTED:
    Serial.println("Ethernet Connected");

    break;

  case SYSTEM_EVENT_ETH_GOT_IP:
    Serial.print("ETH MAC: ");
    Serial.print(ETH.macAddress());
    Serial.print(", IPv4: ");
    Serial.print(ETH.localIP());
    if (ETH.fullDuplex()) {
      Serial.print(", FULL_DUPLEX");
    }
    Serial.print(", ");
    Serial.print(ETH.linkSpeed());
    Serial.println("Mbps");

    ethernetConnected = true;

    connectToMqtt();

    break;

  case SYSTEM_EVENT_ETH_DISCONNECTED:
  case SYSTEM_EVENT_ETH_STOP:
    Serial.println("Ethernet lost connection");

    ethernetConnected = false;

    xTimerStop(mqttReconnectTimer, 0);
    xTimerStart(ethReconnectTimer, 0);

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

  // Mqtt was disconnected and ethernet is connected, try to reconnect to mqtt
  if (ethernetConnected) {
    xTimerStart(ethReconnectTimer, 0);
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

      // Set the door state to waiting to open
      doorState = DOOR_WAITING;
    } else {
      Serial.println("Unknown payload");
    }
  } else {
    Serial.println("Unknown topic");
  }
}

void onDoorStateChange() {
  uint32_t now = millis();

  // Debounce the hall effect state change
  if (now - doorChangedTime > DOOR_HALL_EFFECT_TIMEOUT) {
    // Mark the door state as changed
    doorChangedState = true;

    doorChangedTime = now;
  }
}

void setup() {
  // Initialize Serial, SPI and RFID
  Serial.begin(115200);
  delay(2000);
  Serial.println("Morning");
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSS_PIN);
  rfid.PCD_Init();

  // Initialze the pins
  pinMode(DOOR_STRIKE_PIN, OUTPUT);
  pinMode(DOOR_HALL_EFFECT_PIN, INPUT_PULLUP);

  // Attach the interrupt
  attachInterrupt(digitalPinToInterrupt(DOOR_HALL_EFFECT_PIN),
                  onDoorStateChange, CHANGE);

  // Initialize crcs
  lockTopicHash = crc32String(MQTT_LOCK_TOPIC);
  lockResponseHashes[LOCK_RESPONSE_NEWCARD] = crc32String("NEWCARD");
  lockResponseHashes[LOCK_RESPONSE_NOACCESS] = crc32String("NOACCESS");
  lockResponseHashes[LOCK_RESPONSE_UNPAID] = crc32String("UNPAID");
  lockResponseHashes[LOCK_RESPONSE_GRANTED] = crc32String("GRANTED");

  // Setup timers for mqtt and ethernet reconnection
  mqttReconnectTimer =
      xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                   reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  ethReconnectTimer = xTimerCreate(
      "ethTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
      reinterpret_cast<TimerCallbackFunction_t>(connectToEthernet));

  // Bind to the wifi events
  WiFi.onEvent(WiFiEvent);

  // Initialize the mqtt client
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  // Connect to the ethernet
  connectToEthernet();
}

void loop() {
  // Has the door state changed?
  if (doorChangedState) {
    // Set the status to open or closed
    const char *status = digitalRead(DOOR_HALL_EFFECT_PIN) ? "open" : "closed";

    // Send status via mqtt
    mqttClient.publish(MQTT_STATE_TOPIC, 1, false, status);
    Serial.println(status);

    // Mark the changed state as false
    doorChangedState = false;
  }

  // Check the door state
  if (doorState == DOOR_WAITING) {
    // Switch the latch to high
    digitalWrite(DOOR_STRIKE_PIN, HIGH);

    // Open the door
    doorState = DOOR_OPENED;
    doorOpenTime = millis();
  } else if (doorState == DOOR_OPENED) {
    // Has the strike timeout elapsed?
    if (millis() - doorOpenTime > DOOR_STRIKE_TIMEOUT) {
      // Switch the latch to low
      digitalWrite(DOOR_STRIKE_PIN, LOW);

      // Close the door
      doorState = DOOR_CLOSED;
    }
  }

  // Wait for new card scan and read it
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }
  Serial.println("Card found");

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
  Serial.println(rfidStr);

  // Halt picc
  rfid.PICC_HaltA();

  // Stop encryption on pcd
  rfid.PCD_StopCrypto1();
}
