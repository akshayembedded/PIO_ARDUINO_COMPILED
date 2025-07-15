#include <Arduino.h>
#include "Global/global.h"
#include "driver/i2c_master.h"
#include "esp_task_wdt.h"

// void setup()
// {
//     // Configure the wrapper
//     Serial.begin(115200);

//     initializeDevice();
//     lua_setup();
    
// }

// void loop()
// {
//     // Handle serial commands
//     processBLEMessages();
//     bleController.processOTAData();
//      delay(10); // <<< Important!

// }

extern "C" void app_main(void) {
    initArduino(); // Init Arduino internals
    // setup();       // Your Arduino setup()
     Serial.begin(115200);

    initializeDevice();
    lua_setup();
    esp_task_wdt_add(NULL);
    while (true) {
        // loop();    // Your Arduino loop()
        // delay or yield automatically handled
     processBLEMessages();
    bleController.processOTAData();
    esp_task_wdt_reset();
        vTaskDelay(10); // FreeRTOS delay (1000ms)
    }
}