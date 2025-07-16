#include <Arduino.h>
#include "Global/global.h"

extern "C" void app_main(void) {
    initArduino(); 
     Serial.begin(115200);
     Serial.println("Starting LuaBLE...");
    initializeDevice();
    lua_setup();
    while (true) {
     processBLEMessages();
    bleController.processOTAData();
    }
}
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
// void myLoopTask(void *param)
// {

//     // Init TWDT
//     while (true) {
//         processBLEMessages();
//         bleController.processOTAData();
//         // No delay here if needed
//     }
// }


