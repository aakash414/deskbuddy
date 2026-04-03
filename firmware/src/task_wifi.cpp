#include <Arduino.h>
#include "shared_state.h"
#include "sensors.h"
#include "server_link.h"
#include "wifi_manager.h"

extern Sensors     sensors;
extern ServerLink  network;
extern WiFiManager wifiManager;

void taskWifi(void* param) {
    wifiManager.begin();
    network.begin(wifiManager.getServerUrl(), wifiManager.getApiKey());

    TickType_t lastSensor = 0;
    TickType_t lastStatus = 0;

    for (;;) {
        // In AP mode nothing to do — config UI task handles the web server
        if (gApMode) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        TickType_t now = xTaskGetTickCount();

        if ((now - lastSensor) >= pdMS_TO_TICKS(SENSOR_INTERVAL)) {
            sensors.read();
            lockState();
            gSensors = { sensors.moisture, sensors.light, sensors.isTouchActive() };
            unlockState();
            network.postSensors(sensors.moisture, sensors.light, sensors.isTouchActive());
            Serial.printf("[wifi] Sensors: moisture=%.0f%% light=%.0f lux\n",
                          sensors.moisture, sensors.light);
            lastSensor = now;
        }

        if ((now - lastStatus) >= pdMS_TO_TICKS(STATUS_INTERVAL)) {
            StatusData data = network.fetchStatus();
            if (data.valid) {
                lockState();
                gStatus = data;
                unlockState();
                Serial.printf("[wifi] Status: state=%s\n", data.state.c_str());
            }
            lastStatus = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
