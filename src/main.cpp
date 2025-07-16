#include <Arduino.h>
#include "Global/global.h"
#include "driver/i2c_master.h"
#include "esp_task_wdt.h"

// extern "C" void app_main(void)
// {
//     // Create config struct
//     // Your custom task code
//     initArduino();
//     Serial.begin(115200);
//     initializeDevice();
//     lua_setup();
//     while (true) {
//         processBLEMessages();
//         bleController.processOTAData();
       
       
//     }
// }

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
void myLoopTask(void *param)
{

    // Init TWDT
    while (true) {
        processBLEMessages();
        bleController.processOTAData();
        // No delay here if needed
    }
}


extern "C" void app_main(void) {
    initArduino(); // Init Arduino internals
    // setup();       // Your Arduino setup()
     Serial.begin(115200);
     Serial.println("Starting LuaBLE...");

    initializeDevice();
    lua_setup();
    // xTaskCreatePinnedToCore(myLoopTask, "LoopTask", 8192, NULL, 1, NULL, 1);
    // esp_task_wdt_add(NULL);
    while (true) {
    //     // loop();    // Your Arduino loop()
    //     // delay or yield automatically handled
     processBLEMessages();
    bleController.processOTAData();
    // // yield(); // or delay(1);
    // esp_task_wdt_reset();
    // //     vTaskDelay(10); // FreeRTOS delay (1000ms)
    }
}