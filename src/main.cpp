#include <Arduino.h>
#include "Global/global.h"


void setup()
{
    // Configure the wrapper
    Serial.begin(115200);

    initializeDevice();
    lua_setup();
    
}

void loop()
{
    // Handle serial commands
    processBLEMessages();
    bleController.processOTAData();
     delay(10); // <<< Important!

}


