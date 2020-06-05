#include <ETH.h>
#include <SPI.h>

#include <AsyncMqttClient.h>
#include <ErriezCRC32.h>
#include <MFRC522.h>

#include "config.h"
#include "utils.h"
#include "debug.h"

#define DEBUG 1

#define MQTT_RFID_TOPIC DEVICE_TYPE "/" DEVICE_ID "/rfid"
#define MQTT_LOCK_TOPIC DEVICE_TYPE "/" DEVICE_ID "/lock"
#define MQTT_STATE_TOPIC DEVICE_TYPE "/" DEVICE_ID "/state"
#define MQTT_LOG_TOPIC DEVICE_TYPE "/" DEVICE_ID "/log"
#define MQTT_RESTART_TOPIC DEVICE_TYPE "/" DEVICE_ID "/restart"

AsyncMqttClient mqttClient;
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
uint32_t restartTopicHash;
uint32_t lockResponseHashes[MAX_LOCK_RESPONSES];

bool ethernetConnected = false;


void connectToMqtt()
{
  //DebugPrintln("Connecting to MQTT...");
    mqttClient.connect();
}

/**********************************************************************
* Wifi&Ethernet connection events
**********************************************************************/
void WiFiEvent(WiFiEvent_t event)
{
    switch (event) {
        case SYSTEM_EVENT_ETH_START:
            DebugPrintln("ETH Started");
            ETH.setHostname(DEVICE_ID);              //set eth hostname here
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
            /***************************************************************************
                We have an IP Connect MQTT
            ***************************************************************************/
            connectToMqtt();      
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            DebugPrintln("ETH Disconnected");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            DebugPrintln("ETH Stopped");
            break;
        default:
            break;
    }
}

static int8_t lastDisconnectReason = int(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);

void onMqttConnect(bool sessionPresent)
{
    mqttClient.publish(MQTT_LOG_TOPIC, 0, 1, "{ \"available\": 1 }"); // Retain the state topic

    mqttClient.subscribe(MQTT_RESTART_TOPIC, 0);
    mqttClient.subscribe(MQTT_LOCK_TOPIC, 1);

    DebugPrintln("[INFO]: mqtt, Connected as " DEVICE_ID);
    DebugPrintln("[INFO]: mqtt, Subscribed: " MQTT_RESTART_TOPIC);
    DebugPrintln("[INFO]: mqtt, Subscribed: " MQTT_LOCK_TOPIC);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    if (lastDisconnectReason != int(reason)){
        switch (reason) {
            case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
                DebugPrintln("[WARN]: mqtt, TCP disconnect");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
                 DebugPrintln("[WARN]: mqtt, Bad Protocol");
                 break;
            case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
                DebugPrintln("[WARN]: mqtt, Bad Client ID");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
                DebugPrintln("[WARN]: mqtt, unavailable");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
                DebugPrintln("[WARN]: mqtt, Bad Credentials");
                break;
            case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
                DebugPrintln("[WARN]: mqtt, Unauthorized");
                break;
            case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
                DebugPrintln("[ERR]: mqtt, Bad Fingerprint");
                break;
        }
        lastDisconnectReason = int(reason);
    }
}


/**********************************************************************
* Callback for any MQTT message recieved
**********************************************************************/
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
      DebugPrintln("NEWCARD");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_NOACCESS]) {
      DebugPrintln("NOACCESS");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_UNPAID]) {
      DebugPrintln("UNPAID");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_GRANTED]) {
      DebugPrintln("GRANTED");

      // Set the door state to waiting to open
      doorState = DOOR_WAITING;
    } else {
      DebugPrintln("Unknown payload");
    }
  } else if(topicHash == restartTopicHash){
      mqttClient.publish(MQTT_LOG_TOPIC, 0, 0, "{ \"Restart\": \"onMqttMessage\" }");
      mqttClient.disconnect();
      delay(1000);
      ESP.restart();
      delay(10000);
  } else {
    DebugPrintln("Unknown topic");
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


void setup()
{

    DebugBegin(115200);

    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSS_PIN);
    rfid.PCD_Init();

    // Initialze the pins
    pinMode(DOOR_STRIKE_PIN, OUTPUT);
    pinMode(DOOR_HALL_EFFECT_PIN, INPUT_PULLUP);

    // Attach the interrupt
    attachInterrupt(digitalPinToInterrupt(DOOR_HALL_EFFECT_PIN), onDoorStateChange, CHANGE);

    // Initialize crcs
    lockTopicHash = crc32String(MQTT_LOCK_TOPIC);
    restartTopicHash = crc32String(MQTT_RESTART_TOPIC);
    lockResponseHashes[LOCK_RESPONSE_NEWCARD] = crc32String("NEWCARD");
    lockResponseHashes[LOCK_RESPONSE_NOACCESS] = crc32String("NOACCESS");
    lockResponseHashes[LOCK_RESPONSE_UNPAID] = crc32String("UNPAID");
    lockResponseHashes[LOCK_RESPONSE_GRANTED] = crc32String("GRANTED");

    // Bind to the wifi events
    WiFi.onEvent(WiFiEvent);

    // Initialize the mqtt client
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setClientId(DEVICE_ID);
    // mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
    mqttClient.setWill(MQTT_LOG_TOPIC, 0, 1, "{ \"available\": 0 }"); //const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    // Connect to the ethernet
    ETH.begin();
}


void loop()
{
    if(!mqttClient.connected()){
        connectToMqtt();
    }
      // Has the door state changed?
    if (doorChangedState) {
    // Set the status to open or closed
        const char *status = digitalRead(DOOR_HALL_EFFECT_PIN) ? "open" : "closed";

        // Send status via mqtt
        mqttClient.publish(MQTT_STATE_TOPIC, 1, false, status);
        DebugPrintln(status);

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
    DebugPrintln("Card found");

    // Make sure card is a mifare card
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
        piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
        piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        DebugPrintln(F("Your tag is not of type MIFARE Classic."));

        return;
    }

    // Convert binary rfid to hex string
    char rfidStr[64] = {0};
    hexToAscii(rfid.uid.uidByte, rfidStr, rfid.uid.size);

    // Send rfid via mqtt
    mqttClient.publish(MQTT_RFID_TOPIC, 1, false, rfidStr);
    DebugPrintln(rfidStr);

    // Halt picc
    rfid.PICC_HaltA();

    // Stop encryption on pcd
    rfid.PCD_StopCrypto1();
}
