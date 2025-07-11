#ifndef DEVICE_INIT_H
#define DEVICE_INIT_H

#include <Arduino.h>
#include "Global/global.h"
// Main initialization function
void initializeDevice();

void initializeBLEController(String deviceName="");
void intializeStorage();

#endif // DEVICE_INIT_H