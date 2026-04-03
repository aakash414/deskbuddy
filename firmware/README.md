# DeskBuddy Firmware

ESP32 firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.8 board.

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB-C cable connected to the Waveshare board
- Waveshare board support files (see below)

## Board Setup

The Waveshare ESP32-S3-Touch-AMOLED-1.8 uses custom drivers:
- **SH8601** AMOLED display via QSPI
- **FT3168** capacitive touch via I2C
- **AXP2101** power management

Download the Waveshare example code from their wiki and copy the board
driver files into `include/`. The main entry in `src/main.cpp` has
comments marking where the board init goes.

## Wiring

External sensors connected to the board's exposed GPIO pads:

| Sensor | Pin | Notes |
|--------|-----|-------|
| Capacitive Moisture v2.0 signal | GPIO17 | ADC2 ch6, with retry logic |
| BH1750 SDA | GPIO15 | Shared I2C via exposed pads |
| BH1750 SCL | GPIO14 | Shared I2C via exposed pads |
| Moisture VCC | 3V3 pad | |
| BH1750 VCC | 3V3 pad | |
| Both GND | GND pad | |

## Build & Flash

```bash
cd firmware

# Edit WiFi credentials
# Open include/config.h and set WIFI_SSID, WIFI_PASSWORD, SERVER_IP

# Build
pio run

# Flash
pio run --target upload

# Monitor serial output
pio device monitor
```

## Sprite Pipeline

1. Design 48x48 sprites in Piskel (piskelapp.com)
2. Export as horizontal PNG strip (e.g. 192x48 for 4 frames)
3. Convert to C header:

```bash
python ../scripts/png_to_rgb565.py happy_strip.png --name happy --frames 4 --size 48
```

4. Copy generated `.h` file to `include/sprites/`
5. Rebuild firmware

## Calibrating Moisture Sensor

The capacitive sensor outputs different voltages depending on your soil
and pot. To calibrate:

1. Flash firmware, open serial monitor
2. Read ADC value with sensor in air → set as `MOISTURE_AIR` in config.h
3. Read ADC value with sensor in water → set as `MOISTURE_WATER`
4. Rebuild and flash
