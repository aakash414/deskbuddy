#pragma once

// WiFi — fill in before flashing
#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// Server
#define SERVER_HOST   "deskbuddy.local"
#define SERVER_PORT   3777
// Fallback IP if mDNS fails — set this to your Mac Mini's IP
#define SERVER_IP     "192.168.1.100"

// Timing (ms)
#define SENSOR_INTERVAL    10000   // Read sensors every 10s
#define STATUS_INTERVAL    30000   // Poll /status every 30s
#define FRAME_INTERVAL_DEFAULT 300 // Animation frame rate
#define OFFLINE_TIMEOUT    300000  // 5 min without server = offline

// Pins - Waveshare ESP32-S3-Touch-AMOLED-1.8
// Display is driven internally by the board (SH8601 AMOLED via QSPI)
// Exposed GPIO we use:
#define PIN_MOISTURE   17   // ADC2 channel 6 - analog soil moisture
#define PIN_I2C_SDA    15   // I2C data  (BH1750)
#define PIN_I2C_SCL    14   // I2C clock (BH1750)

// BH1750 I2C address
#define BH1750_ADDR    0x23

// Moisture sensor calibration
// Capacitive sensor v2.0 outputs ~1.2V (wet) to ~2.8V (dry) on 3.3V
// ESP32-S3 ADC: 0-4095 for 0-3.3V
#define MOISTURE_AIR    2800  // ADC reading in air (dry)
#define MOISTURE_WATER  1200  // ADC reading submerged (wet)

// Display - Waveshare 1.8" AMOLED
#define SCREEN_WIDTH   368
#define SCREEN_HEIGHT  448

// Sprite
#define SPRITE_SIZE    48    // 48x48 base sprite
#define SPRITE_SCALE   7     // Scaled to 336px on screen
