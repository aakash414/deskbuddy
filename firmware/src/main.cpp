#include <Arduino.h>
#include <lvgl.h>
#include "Waveshare_AMOLED.h"
#include "config.h"
#include "sensors.h"
#include "network.h"
#include "display.h"

// Waveshare board instance
static AMOLED_1_8 amoled;

// LVGL draw buffer — two 1/10-screen buffers in PSRAM
static lv_color_t* lvBuf1;
static lv_color_t* lvBuf2;

Sensors sensors;
Network network;
Display display;

unsigned long lastSensorRead = 0;
unsigned long lastStatusFetch = 0;
StatusData lastStatus;

// ── LVGL display flush callback ──────────────────────────────────────────────
void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    amoled.pushColors(area->x1, area->y1, w, h, (uint16_t*)px_map);
    lv_display_flush_ready(disp);
}

// ── LVGL touch read callback ─────────────────────────────────────────────────
void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    int16_t x = 0, y = 0;
    if (amoled.getTouch(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        sensors.setTouched(true);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        sensors.setTouched(false);
    }
}

// ── Display driver init ──────────────────────────────────────────────────────
void initDisplay() {
    // Init board hardware (SH8601 AMOLED + FT3168 touch + AXP2101 PMIC)
    amoled.begin();

    // Allocate LVGL draw buffers in PSRAM
    size_t bufSize = SCREEN_WIDTH * (SCREEN_HEIGHT / 10) * sizeof(lv_color_t);
    lvBuf1 = (lv_color_t*)ps_malloc(bufSize);
    lvBuf2 = (lv_color_t*)ps_malloc(bufSize);
    if (!lvBuf1 || !lvBuf2) {
        Serial.println("FATAL: LVGL buffer alloc failed — no PSRAM?");
        while (true) delay(1000);
    }

    // Register display with LVGL
    lv_display_t* disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_draw_buffers(disp, lvBuf1, lvBuf2, bufSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Register touch input
    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, lvgl_touch_cb);

    Serial.println("Display driver initialized");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== DeskBuddy v1.0 ===");

    lv_init();
    initDisplay();

    // Build our UI on top of LVGL
    display.begin();

    // Initialize sensors
    sensors.begin();

    // Connect to WiFi and resolve server
    network.begin();

    Serial.println("Setup complete\n");
}

void loop() {
    // LVGL tick
    lv_timer_handler();

    // Read sensors every 10s
    if (millis() - lastSensorRead > SENSOR_INTERVAL) {
        sensors.read();
        lastSensorRead = millis();

        Serial.printf("Sensors: moisture=%.0f%% light=%.0f lux touched=%d\n",
            sensors.moisture, sensors.light, sensors.isTouchActive());

        network.postSensors(sensors.moisture, sensors.light, sensors.isTouchActive());
    }

    // Fetch status every 30s
    if (millis() - lastStatusFetch > STATUS_INTERVAL) {
        StatusData data = network.fetchStatus();
        lastStatusFetch = millis();

        if (data.valid) {
            lastStatus = data;
            display.update(data);
            Serial.printf("Status: state=%s location=%s\n",
                data.state.c_str(), data.locationLabel.c_str());
        }
    }

    // Handle offline state
    if (network.isOffline()) {
        display.showOffline();
    }

    // WiFi maintenance
    network.maintain();

    // Display animation tick
    display.tick();

    delay(5);
}
