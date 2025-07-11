#ifndef GLOBAL_H
#define GLOBAL_H

#include "config.h"
#include "lua_setup/lua_setup.h"
#include <BLEController.h>
#include "Storage/storage.h"
#include "ble_handlers.h"
#include "LuaQueue/lqueue.h"
#include "device_init.h"
#include "buzzer/piezo32.h"

#define USE_HSPI_PORT

#include <SPI.h>
#include <Wire.h>




// Lidar lidarBottom(Lidar_Bottom_sda, Lidar_Bottom_scl); // sda,scl
// Lidar lidarTop(Lidar_Top_sda, Lidar_Top_scl);


#endif // GLOBAL_H