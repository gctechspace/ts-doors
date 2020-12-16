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

#define DOOR_STRIKE_PIN     12
#define DOOR_STRIKE_TIMEOUT 4000

#define DOOR_HALL_EFFECT_PIN        14
#define DOOR_HALL_EFFECT_TIMEOUT    100

/* 
   * ETH_CLOCK_GPIO0_IN   - default: external clock from crystal oscillator
   * ETH_CLOCK_GPIO0_OUT  - 50MHz clock from internal APLL output on GPIO0 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO16_OUT - 50MHz clock from internal APLL output on GPIO16 - possibly an inverter is needed for LAN8720
   * ETH_CLOCK_GPIO17_OUT - 50MHz clock from internal APLL inverted output on GPIO17 - tested with LAN8720
*/

#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN

// Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define ETH_POWER_PIN   -1

// Type of the Ethernet PHY (LAN8720 or TLK110)
#define ETH_TYPE        ETH_PHY_LAN8720

// I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_ADDR        0

// Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_MDC_PIN     16

// Pin# of the I²C IO signal for the Ethernet PHY
#define ETH_MDIO_PIN    17