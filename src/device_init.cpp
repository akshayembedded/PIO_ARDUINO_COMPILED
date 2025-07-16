#include "Global/global.h"

void initializeDevice()
{

    intializeStorage();
    initializeBLEController();
    piezo_init_c();
    piezo_set_speed_c(160); // Set default speed to 80ms
    piezo_play_music_c("A2D2F2A2");

}

void initializeBLEController(String deviceName)
{

    bleController.setBlePrefix(BLE_NAME_PREFIX);
    bleController.setBleProductUUID(BLE_PRODUCT_UUID);
    bleController.setOnConnectCallback(handleBleConnect);
    bleController.setOnDisconnectCallback(handleBleDisconnect);
    bleController.setTextMessageCallback(lua_loop);
    bleController.setTextAbortCallback(luaClose);
    bleController.setOtaCallbacks(onOtaStart, onOtaProgress, onOtaSuccess, onOtaError);
    bleController.setTextQueueCallback(addStringToQueue);
    bleController.begin(deviceName);
    bleController.switchToTextMode();
    initializeBLEHandlers();
    initQueue();
    LLOGI("BLE Controller initialized");
}

void intializeStorage()
{

    STORAGE.initPreferences();
}
