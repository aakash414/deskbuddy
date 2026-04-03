#pragma once

// WiFi — fill in before flashing
#define WIFI_SSID "HABITAT 002"
#define WIFI_PASSWORD "002002002"

// Server
#define SERVER_HOST "deskbuddy.local"
#define SERVER_PORT 3777
// Fallback IP if mDNS fails — set this to your Mac Mini's IP
#define SERVER_IP "192.168.1.100"
// API key — must match API_KEY in server .env
#define API_KEY "your_generated_key_here"

// Timing (ms)
#define SENSOR_INTERVAL 10000      // Read sensors every 10s
#define STATUS_INTERVAL 30000      // Poll /status every 30s
#define FRAME_INTERVAL_DEFAULT 300 // Animation frame rate
#define OFFLINE_TIMEOUT 300000     // 5 min without server = offline

// Pins - Waveshare ESP32-S3-Touch-AMOLED-1.8
// LCD QSPI (hardwired on PCB)
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_WIDTH 368
#define LCD_HEIGHT 448

// I2C bus (shared: BH1750 light sensor + FT3168 touch controller)
#define PIN_I2C_SDA 15
#define PIN_I2C_SCL 14
#define PIN_TOUCH_INT 21

// Exposed GPIO we use:
#define PIN_MOISTURE 17 // ADC2 channel 6 - analog soil moisture

// BH1750 I2C address
#define BH1750_ADDR 0x23

// Moisture sensor calibration
// Capacitive sensor v2.0 outputs ~1.2V (wet) to ~2.8V (dry) on 3.3V
// ESP32-S3 ADC: 0-4095 for 0-3.3V
#define MOISTURE_AIR 2800   // ADC reading in air (dry)
#define MOISTURE_WATER 1200 // ADC reading submerged (wet)

// Sprite
#define SPRITE_SIZE 48 // 48x48 base sprite
#define SPRITE_SCALE 6 // Scaled to 288px on screen (48*6)

// AP mode (used when WiFi connection fails)
#define WIFI_AP_SSID "DeskBuddy-Setup"
#define WIFI_AP_PASSWORD "" // open network

// FreeRTOS task stack sizes (bytes)
#define STACK_DISPLAY 20480 // needs headroom for LittleFS file reads
#define STACK_WIFI 16384    // WiFi init needs extra headroom
#define STACK_CONFIG_UI 6144
