#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>

//BLe
#define BLE_PRODUCT_UUID "AE06"
#define BLE_NAME_PREFIX "HYPERLAB"


//Buzzer
#define PIEZO_PIN 48  // Default GPIO pin for piezo speaker
#define PIEZO_CHANNEL 4  // Default LEDC channel


//SPi Display
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define DISPLAY_ROTATION 0
#define DISPLAY_MOSI 40
#define DISPLAY_MISO 41
#define DISPLAY_SCK 42
#define DISPLAY_CS 04
#define DISPLAY_DC 05
#define DISPLAY_RST 06
#define DISPLAY_BL 07

//RFID
#define RFID_RST 38
#define RFID_IRQ 39
#define RFID_MOSI 40
#define RFID_MISO 41
#define RFID_SCK 42
#define RFID_CS 02



#endif