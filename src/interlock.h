
#include <ErriezCRC32.h>

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

bool validmessage(const char *topic, char *payload){
    // Calculate the crc for the topic
  uint32_t topicHash = crc32String(topic);

  // Ensure the message came from our lock topic
  if (topicHash == lockTopicHash) {
    // Calculate the crc for the payload
    uint32_t payloadHash = crc32Buffer(payload, strlen(payload));

    // Check payload hash against known response hashes
    if (payloadHash == lockResponseHashes[LOCK_RESPONSE_NEWCARD]) {
      DebugPrintln("NEWCARD");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_NOACCESS]) {
      DebugPrintln("NOACCESS");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_UNPAID]) {
      DebugPrintln("UNPAID");
    } else if (payloadHash == lockResponseHashes[LOCK_RESPONSE_GRANTED]) {
      DebugPrintln("GRANTED");

      return true;
    } else {
      DebugPrint("Unknown payload: ");
      DebugPrintln(payload);
    }
  } else {
    DebugPrint("Unknown topic: ");
    DebugPrintln(topic);
  }
  DebugPrintln(payload); 
  return false;
}

void interlocksetup(){
  // Initialize crcs
  lockTopicHash = crc32String(MQTT_LOCK_TOPIC);
  restartTopicHash = crc32String(MQTT_RESTART_TOPIC);
  lockResponseHashes[LOCK_RESPONSE_NEWCARD] = crc32String("NEWCARD");
  lockResponseHashes[LOCK_RESPONSE_NOACCESS] = crc32String("NOACCESS");
  lockResponseHashes[LOCK_RESPONSE_UNPAID] = crc32String("UNPAID");
  lockResponseHashes[LOCK_RESPONSE_GRANTED] = crc32String("GRANTED");
}