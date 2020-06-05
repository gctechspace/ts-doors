#pragma once

#define DEVICE_TYPE "doors"
#define DEVICE_ID "xxxxx"

#define MQTT_HOST IPAddress(172, 16, 2, 2)
#define MQTT_PORT 1883

#define RFID_RST_PIN    4
#define RFID_SS_PIN     13

#define MISO_PIN      32
#define MOSI_PIN      23
#define SCK_PIN       18
#define CSS_PIN       13

#define DOOR_STRIKE_PIN     36
#define DOOR_STRIKE_TIMEOUT 4000

#define DOOR_HALL_EFFECT_PIN        39
#define DOOR_HALL_EFFECT_TIMEOUT    100