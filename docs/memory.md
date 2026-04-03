# DeskBuddy — Project Memory

## What is it
A desk device at Aakash's WeWork office. A 1.8" AMOLED screen shows an animated pixel-art plant-ghost mascot that reflects plant health (soil moisture, light) and work context (location, Jira task, Google Calendar meetings). Teammates glance at it to see where Aakash is without Slack.

## Final architecture
Three devices on WeWork WiFi:
- **MacBook Air M2** — location beacon only. BSSID daemon (LaunchAgent, every 30s) reads WiFi access point, maps to location, POSTs to Mac Mini
- **Mac Mini** (always on at office) — Node.js + Express at deskbuddy.local:3777. Polls Google Calendar (60s), Jira (60s), receives location + sensor data, runs state engine, serves GET /status
- **Waveshare ESP32-S3-Touch-AMOLED-1.8** — display + sensors. Reads moisture + light every 10s, POSTs to Mac Mini. GETs /status every 30s, renders sprite + text

## Hardware (final BOM — 3 items to buy)
1. **Waveshare ESP32-S3-Touch-AMOLED-1.8** (~₹2,713 from hubtronics.in) — all-in-one: ESP32-S3R8, 1.8" AMOLED 368×448, capacitive touch (FT3168), mic, speaker, IMU, RTC, case included
2. **Capacitive Soil Moisture Sensor v2.0** (~₹150-200 from robu.in) — 3.3-5.5V, analog output, connects to GPIO17 (ADC2 ch6)
3. **GY-302 BH1750 Light Sensor** (~₹120-180 from robu.in) — I2C (0x23), 3-5V, connects to board's I2C pads (GPIO14 SCL, GPIO15 SDA)

Already owned: Mac Mini, MacBook Air M2, plant + pot

## Key decisions made
- Mac Mini replaces Raspberry Pi (already owned, more powerful)
- BME280 temperature sensor dropped — not used in any state logic or display
- Claude AI rubber duck feature removed from v1 — documented as future
- No speaker/mic usage in v1 (board has them for future)
- Capacitive moisture sensor on GPIO17 (ADC2) — works on ESP32-S3 with hardware arbiter even with WiFi active (NOT the same as original ESP32 hard block). Retry logic needed for occasional timeouts.
- 1.8" AMOLED is fine for team visibility — teammates sit within 1 meter

## Display layout (final)
- Sprite fills ~80% of screen (scaled 9.5× from base grid, ~20mm tall on physical display)
- One text line at bottom alternating every 5s between location ("At desk") and state ("Focused")
- Plant health via ambient edge glow: green = healthy (subtle), red = needs water (prominent)
- No dots, no icons, no labels, no task names, no temperature — maximum simplicity
- AMOLED true blacks make unused areas invisible
- Physical display: 28.7mm × 34.9mm. Case: 37.6mm × 45.2mm

## Exposed GPIO pinout (from board image)
VBUS, GND, 3V3, GND, TXD, RXD, SCL(GPIO14), SDA(GPIO15), GPIO17, GPIO18, GPIO38, GPIO39, GPIO40, GPIO41, GPIO42, USB_P(GPIO20), USB_N(GPIO19)

## I2C address map (no conflicts)
- BH1750: 0x23 (external, on exposed I2C pads)
- ES8311: 0x18 (onboard)
- TCA9554: 0x20 (onboard IO expander)
- AXP2101: 0x34 (onboard power)
- FT3168: 0x38 (onboard touch)
- PCF85063: 0x51 (onboard RTC)
- QMI8658: 0x6B (onboard IMU)

## ADC2 + WiFi on ESP32-S3 (resolved)
GPIO17 is ADC2 Channel 6. On ESP32-S3, ADC2 has a hardware arbiter (unlike original ESP32 where it hard-blocks). WiFi has higher priority but ADC2 reads still work — up to 1000 samples/sec with WiFi on per Espressif FAQ. Reads may occasionally timeout; use retry logic. This is NOT the same as the original ESP32 limitation.

## WiFi location tracking (validated March 20, 2026)
4 distinct BSSIDs at WeWork office, all 5GHz:
- desk: -59 dBm, ch161 (strong)
- cubicle: -78 dBm, ch56 (weak)
- roof: -66 dBm, ch112 (moderate)
- office_floor_cubicle: -75 dBm, ch161 (different floor)

## State engine (10 states, priority cascade)
1. Plant emergency (moisture <20 → thirsty, >90 → overwatered)
2. Manual override (from dashboard)
3. Touch sensor (screen tap → loved, 10s)
4. Darkness (light <50 lux → sleeping)
5. User away (no location 5min → sleeping)
6. Calendar event (inMeeting → meeting)
7. Location (roof → rooftop, cubicle → focus)
8. Jira activity (in-progress → coding, none → idle)
9. Fallback → happy

## Character design
Plant-ghost hybrid. Same silhouette every state. Differentiation via body color, eye expression, leaf style, accessories (headphones, sunglasses, speech bubble), effects (sparkles, hearts, tear, wind), animation speed.

## API endpoints
- GET /status — ESP32 polls every 30s
- POST /sensors — ESP32 pushes every 10s
- POST /location — MacBook daemon every 30s
- POST /override — manual state force
- GET /dashboard — debug web UI
- GET /health — server health check

## 9-day build plan
Day 1: Mac Mini server scaffold + Waveshare board test
Day 2: Sprites (48×48 Piskel) + LVGL display layout
Day 3: Plant sensors + state engine
Day 4: Animations + transitions
Day 5: Google Calendar + Jira integration
Day 6: WiFi BSSID location tracking
Day 7: Dashboard + manual override
Day 8: Edge cases + reliability
Day 9: Final testing + deploy

## Documents created
- DeskBuddy-Specification.md — clean standalone spec (final, no version history)
- DeskBuddy-Technical-Specification-v4.md — detailed versioned spec
- DeskBuddy-Proposal.pdf — PDF proposal (no code blocks)
- deskbuddy-wifi-scout.sh — WiFi BSSID mapping script
- deskbuddy-v3.html — interactive character preview

## Future enhancements (not in v1)
- Claude AI rubber duck via web chat UI
- Voice interaction (onboard mic + speaker available)
- Slack status sync
- GitHub integration
- Multiple DeskBuddies for teammates
- Motion detection via onboard IMU
- Portable mode via LiPo battery support
- OTA firmware updates
- Custom 3D-printed integrated enclosure
