#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Sensor readings shared between taskWifi (writer) and taskDisplay (reader)
struct SensorData {
    float moisture = 0;
    float light    = 0;
    bool  touched  = false;
};

// Server status shared between taskWifi (writer) and taskDisplay (reader)
struct StatusData {
    String state;
    String locationLabel;
    String alternatingText[2];
    int    altTextCount = 0;
    float  moisture     = 0;
    String moistureStatus;
    float  light        = 0;
    String lightStatus;
    bool   valid        = false;
};

// Globals — defined in main.cpp, declared extern here
extern SemaphoreHandle_t gStateMutex;
extern SensorData        gSensors;
extern StatusData        gStatus;
extern volatile bool     gWifiConnected;
extern volatile bool     gApMode;
extern TaskHandle_t      gConfigUIHandle;
extern String            gDeviceIP;   // set by WiFiManager after connect/AP start

inline void lockState()   { if (gStateMutex) xSemaphoreTake(gStateMutex, portMAX_DELAY); }
inline void unlockState() { if (gStateMutex) xSemaphoreGive(gStateMutex); }
