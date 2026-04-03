#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"

class Sensors {
public:
    float moisture = -1;    // 0-100 percentage
    float light = -1;       // lux
    bool touched = false;

    void begin() {
        // ADC for moisture sensor on GPIO17
        analogSetAttenuation(ADC_11db);
        pinMode(PIN_MOISTURE, INPUT);

        // I2C bus already started by initDisplay() (shared with FT3168 touch controller)
        if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDR, &Wire)) {
            lightReady = true;
            Serial.println("BH1750 initialized");
        } else {
            Serial.println("BH1750 init failed - check wiring");
        }
    }

    void read() {
        readMoisture();
        readLight();
        // Touch is handled by the onboard FT3168 capacitive touch controller
        // via LVGL input driver - we read it from the touch callback
    }

    void setTouched(bool state) {
        touched = state;
        if (state) {
            lastTouchTime = millis();
        }
    }

    bool isTouchActive() {
        // Touch stays active for TOUCH_HOLD seconds after release
        if (touched) return true;
        if (lastTouchTime > 0) {
            uint32_t holdMs = 10000; // 10s hold
            return (millis() - lastTouchTime) < holdMs;
        }
        return false;
    }

private:
    BH1750 lightSensor;
    bool lightReady = false;
    unsigned long lastTouchTime = 0;

    void readMoisture() {
        // ESP32-S3 ADC2 + WiFi: hardware arbiter allows concurrent use
        // but reads may occasionally fail. Retry up to 3 times.
        int raw = 0;
        bool success = false;

        for (int attempt = 0; attempt < 3; attempt++) {
            raw = analogRead(PIN_MOISTURE);
            if (raw > 0) {
                success = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (!success) {
            Serial.println("Moisture: ADC read failed after 3 retries");
            return; // Keep last known value
        }

        // Map ADC to 0-100% (invert: high ADC = dry = low moisture)
        float pct = (float)(MOISTURE_AIR - raw) / (float)(MOISTURE_AIR - MOISTURE_WATER) * 100.0f;
        moisture = constrain(pct, 0.0f, 100.0f);
    }

    void readLight() {
        if (!lightReady) return;

        float lux = lightSensor.readLightLevel();
        if (lux >= 0) {
            light = lux;
        } else {
            Serial.println("BH1750: read failed");
        }
    }
};
