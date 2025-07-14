#include <Arduino.h>
#include "Global/global.h"
#include "driver/i2c_master.h"


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

}

