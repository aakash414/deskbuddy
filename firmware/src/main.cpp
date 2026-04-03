#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <Arduino_GFX.h>
#include <databus/Arduino_ESP32QSPI.h>
#include <display/Arduino_SH8601.h>
#include <Adafruit_XCA9554.h>
#include <LittleFS.h>
#include "config.h"
#include "shared_state.h"
#include "sensors.h"
#include "server_link.h"
#include "wifi_manager.h"
#include "display.h"

// ── Shared state globals (declared extern in shared_state.h) ─────────────────
SemaphoreHandle_t gStateMutex    = nullptr;
SensorData        gSensors;
StatusData        gStatus;
volatile bool     gWifiConnected = false;
volatile bool     gApMode        = false;
TaskHandle_t      gConfigUIHandle = nullptr;
String            gDeviceIP;

// ── App objects ───────────────────────────────────────────────────────────────
Sensors     sensors;
ServerLink  network;
WiFiManager wifiManager;
Display     display;

// ── Hardware objects ──────────────────────────────────────────────────────────
static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
static Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, LCD_WIDTH, LCD_HEIGHT);

static Adafruit_XCA9554 expander;
static lv_disp_draw_buf_t draw_buf;

// ── Task function declarations ────────────────────────────────────────────────
void taskDisplay(void* param);
void taskWifi(void* param);
void taskConfigUI(void* param);

// ── FT3168 touch (direct I2C) ────────────────────────────────────────────────
#define FT3168_ADDR      0x38
#define FT3168_TD_STATUS 0x02
#define FT3168_TOUCH1_XH 0x03
#define FT3168_TOUCH1_XL 0x04
#define FT3168_TOUCH1_YH 0x05
#define FT3168_TOUCH1_YL 0x06

volatile bool touchFlag = false;

void IRAM_ATTR touchISR() {
    touchFlag = true;
}

static bool ft3168_read(int16_t *x, int16_t *y) {
    Wire.beginTransmission(FT3168_ADDR);
    Wire.write(FT3168_TD_STATUS);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)5);
    if (Wire.available() < 5) return false;

    uint8_t count = Wire.read() & 0x0F;
    uint8_t xh    = Wire.read();
    uint8_t xl    = Wire.read();
    uint8_t yh    = Wire.read();
    uint8_t yl    = Wire.read();

    if (count == 0) return false;
    *x = (int16_t)(((xh & 0x0F) << 8) | xl);
    *y = (int16_t)(((yh & 0x0F) << 8) | yl);
    return true;
}

// ── LVGL callbacks ────────────────────────────────────────────────────────────
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp);
}

void lvgl_touch_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    static int16_t lastX = 0, lastY = 0;
    if (touchFlag) {
        touchFlag = false;
        int16_t x, y;
        if (ft3168_read(&x, &y)) {
            lastX = x; lastY = y;
            data->state   = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
            sensors.setTouched(true);
            return;
        }
    }
    data->state   = LV_INDEV_STATE_REL;
    data->point.x = lastX;
    data->point.y = lastY;
    sensors.setTouched(false);
}

// ── Display hardware init (called once from setup) ────────────────────────────
static void initDisplay() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!expander.begin(0x20)) {
        Serial.println("WARN: XCA9554 expander not found");
    } else {
        expander.pinMode(0, OUTPUT); expander.pinMode(1, OUTPUT);
        expander.pinMode(2, OUTPUT); expander.pinMode(6, OUTPUT);
        expander.digitalWrite(0, LOW); expander.digitalWrite(1, LOW);
        expander.digitalWrite(2, LOW); expander.digitalWrite(6, LOW);
        delay(20);
        expander.digitalWrite(0, HIGH); expander.digitalWrite(1, HIGH);
        expander.digitalWrite(2, HIGH); expander.digitalWrite(6, HIGH);
        Serial.println("XCA9554 initialized");
    }

    pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), touchISR, FALLING);

    gfx->begin();
    gfx->setBrightness(200);
    Serial.printf("Display: %dx%d\n", gfx->width(), gfx->height());

    gfx->fillScreen(0xF800);
    delay(1000);
    gfx->fillScreen(0x0000);

    // Prefer PSRAM for LVGL buffers — frees ~160KB of internal SRAM for the WiFi driver.
    // Fallback: use 1/10-screen DMA buffers in internal SRAM if PSRAM is unavailable.
    size_t bufPixels = LCD_WIDTH * LCD_HEIGHT / 4;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = buf1 ? (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM) : nullptr;
    if (!buf1 || !buf2) {
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        bufPixels = LCD_WIDTH * LCD_HEIGHT / 10;
        buf1 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
        buf2 = (lv_color_t *)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
        Serial.printf("[display] PSRAM unavailable, using %u-px DMA buffers\n", (unsigned)bufPixels);
    } else {
        Serial.printf("[display] Using PSRAM buffers (%u px each)\n", (unsigned)bufPixels);
    }
    if (!buf1 || !buf2) {
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        Serial.println("FATAL: LVGL buffer alloc failed");
        Serial.flush();
        while (true) delay(1000);
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, bufPixels);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_WIDTH;
    disp_drv.ver_res  = LCD_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    Serial.println("Display driver initialized");
}

// ── Arduino entry points ──────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    // Lock both cores to 240 MHz — prevents WiFi/BT from downscaling the clock
    setCpuFrequencyMhz(240);
    delay(3000);
    Serial.println("\n=== DeskBuddy v1.0 ===");
    Serial.printf("CPU: %u MHz  Cores: %d\n", getCpuFrequencyMhz(), portNUM_PROCESSORS);
    Serial.printf("PSRAM: %u KB total  %u KB free\n",
                  ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);

    gStateMutex = xSemaphoreCreateMutex();

    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("WARN: LittleFS mount failed — run uploadfs first");
    } else {
        Serial.println("LittleFS mounted");
    }

    lv_init();
    initDisplay();
    display.begin();
    sensors.begin();

    // Core 1 (APP_CPU): display task — owns all LVGL calls, highest priority
    xTaskCreatePinnedToCore(taskDisplay, "display", STACK_DISPLAY, nullptr, 3, nullptr, 1);
    // Core 0 (PRO_CPU): WiFi task — same core as WiFi/BT stack for minimal context-switch overhead
    xTaskCreatePinnedToCore(taskWifi,   "wifi",    STACK_WIFI,    nullptr, 2, nullptr, 0);
    // taskConfigUI created by WiFiManager::startAP() on Core 0 if AP mode activates

    Serial.println("Tasks started\n");
}

void loop() {
    // Arduino loop task not needed — hand off fully to RTOS scheduler
    vTaskDelete(nullptr);
}
