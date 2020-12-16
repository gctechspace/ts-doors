
#include <SPI.h>
#include <MFRC522.h>



MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);

void RFIDSetup(){
  SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, RFID_SDA_PIN);
  rfid.PCD_Init();
}


void RFIDLoop(){
  
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


    messagesend(MQTT_RFID_TOPIC,rfidStr,1 ,false);
    DebugPrintln(rfidStr);

    // Halt picc
    rfid.PICC_HaltA();

    // Stop encryption on pcd
    rfid.PCD_StopCrypto1();
    
}
