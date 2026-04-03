#include <Arduino.h>
#include "wifi_manager.h"

extern WiFiManager wifiManager;

// Runs only in AP mode — started by WiFiManager::startAP().
// Calls handleClient() in a tight loop to serve the config web UI.
// Self-terminates implicitly when ESP.restart() fires inside handleClient().
void taskConfigUI(void* param) {
    for (;;) {
        wifiManager.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
