# DeskBuddy — Technical Specification

---

## 1. What is DeskBuddy?

DeskBuddy is a small desk device that sits next to a plant at your WeWork office. It has a 1.8" AMOLED touch screen showing an animated pixel-art mascot — a plant-ghost character — whose mood and appearance reflect two things: the health of your plant (soil moisture, light, temperature) and your current work context (where you are in the office, what you're working on in Jira, whether you're in a meeting).

When you walk away from your desk to the rooftop or a private cubicle, the mascot automatically detects your location via WiFi fingerprinting and updates itself. Teammates sitting nearby can glance at the display to see where you are and what you're doing — without interrupting you on Slack.

The system has three parts:
- A **Waveshare ESP32-S3-Touch-AMOLED-1.8 board** on the desk — handles the display, touch input, and plant sensors
- A **Mac Mini** at the office (always on) — runs the Node.js API server that aggregates data from Google Calendar, Jira, and the location daemon
- Your **MacBook** — runs a lightweight background script that reports your physical location based on which WiFi access point you're connected to

---

## 2. System architecture

### 2.1 Device topology

```
┌──────────────────┐
│    MacBook        │
│    (moves with    │     POST /location
│     you)          │──────(every 30s)─────────┐
│                   │                           │
│  BSSID location   │     Browser:              │
│  daemon           │──── GET /dashboard ───────┤
│  (LaunchAgent)    │                           │
└──────────────────┘                           │
                                                ▼
                                   ┌────────────────────────┐
                                   │  Mac Mini               │
                                   │  deskbuddy.local:3777   │
                                   │  (always on at WeWork)  │
                                   │                        │
                                   │  Node.js + Express      │
                                   │                        │
                                   │  ● Google Calendar API  │
                                   │  ● Jira REST API       │
                                   │  ● State engine        │
                                   │  ● Sensor data store   │
                                   │  ● Location store      │
                                   └───────────┬────────────┘
                                               │
                                    GET /status │ (every 30s)
                                   POST /sensors│ (every 10s)
                                               │
                                   ┌───────────▼────────────┐
                                   │  Waveshare ESP32-S3    │
                                   │  Touch-AMOLED-1.8      │
                                   │                        │
                                   │  1.8" AMOLED display   │
                                   │  (368×448, QSPI)       │
                                   │  Capacitive touch      │
                                   │                        │
                                   │  External sensors:     │
                                   │  ● Soil moisture       │
                                   │  ● BME280 (temp/humid) │
                                   │  ● BH1750 (light)      │
                                   └────────────────────────┘
```

### 2.2 Device responsibilities

**MacBook — location beacon**

Runs a shell script as a macOS LaunchAgent every 30 seconds. The script reads the BSSID (hardware address) of the WiFi access point you're currently connected to, maps it to a known office location, and POSTs the result to the Mac Mini. This is the only thing the MacBook does for DeskBuddy. If the MacBook sleeps or leaves the office network, the Mac Mini stops receiving location updates and infers you're away after 5 minutes.

**Mac Mini — server and brain**

Always-on Node.js + Express server running at `deskbuddy.local:3777`. It:
- Polls Google Calendar every 60 seconds to know if you're in a meeting
- Polls Jira every 60 seconds to know your current in-progress task
- Receives location updates from the MacBook
- Receives plant sensor readings from the ESP32
- Runs a state priority engine that evaluates all inputs and resolves a single display state
- Serves `GET /status` to the ESP32 — a single JSON payload containing everything the display needs
- Serves a debug dashboard at `/dashboard`

**Waveshare AMOLED board — display and sensors**

Reads soil moisture, temperature, humidity, and light every 10 seconds. Detects touch events on the screen (tap the character to "pet" it). POSTs sensor data to the Mac Mini. Every 30 seconds, fetches `GET /status` and renders the correct character sprite, status text, and plant health indicators on the AMOLED display. The firmware is deliberately simple — a "dumb terminal" that shows what the server tells it to show.

### 2.3 Network

All three devices connect to the same WeWork WiFi network. All communication is HTTP over WiFi:

- MacBook → Mac Mini: `POST /location` every 30s
- ESP32 → Mac Mini: `POST /sensors` every 10s
- ESP32 → Mac Mini: `GET /status` every 30s
- MacBook browser → Mac Mini: `GET /dashboard`
- Mac Mini → External APIs: HTTPS to Google Calendar and Jira

The Mac Mini is discoverable via mDNS as `deskbuddy.local`. The ESP32 resolves this hostname at startup to find the server's IP address.

---

## 3. Hardware

### 3.1 Waveshare ESP32-S3-Touch-AMOLED-1.8

This is the core of the desk device. It is a single all-in-one board that includes:

| Feature | Spec |
|---------|------|
| Processor | ESP32-S3R8, dual-core Xtensa LX7, up to 240MHz |
| Memory | 512KB SRAM, 384KB ROM, 8MB PSRAM, 16MB Flash |
| Display | 1.8" AMOLED, 368×448 pixels, 16.7M colors, QSPI (SH8601 driver) |
| Touch | FT3168 capacitive touch, I2C |
| WiFi | 2.4GHz 802.11 b/g/n, onboard antenna |
| Bluetooth | 5.0 LE |
| IMU | QMI8658 6-axis (3-axis accelerometer + 3-axis gyroscope) |
| RTC | PCF85063, battery-backed |
| Audio | ES8311 codec, onboard microphone, onboard speaker |
| Power management | AXP2101, supports 3.7V LiPo battery (MX1.25 header) |
| USB | Type-C for power and programming |
| Storage | TF card slot |
| Case | Included with the board |

The onboard microphone, speaker, IMU, RTC, and battery support are available but unused in the initial build. They're reserved for future features (voice interaction, motion detection, portable mode).

### 3.2 External sensors

Three sensors connect to the board's GPIO header via I2C and analog:

| Sensor | Model | Interface | I2C Address | What it measures |
|--------|-------|-----------|-------------|-----------------|
| Soil moisture | Capacitive v2.0 | Analog (GPIO) | — | Soil moisture as 0-100% via analog voltage |
| Temperature / humidity | BME280 | I2C | 0x76 | Air temperature (°C), relative humidity (%), barometric pressure |
| Light | BH1750 | I2C | 0x23 | Ambient light in lux |

The BME280 and BH1750 share the same I2C bus with no address conflict. The moisture sensor uses one analog-capable GPIO pin. All three run on 3.3V from the board's header.

### 3.3 Touch input

The "pet the plant" interaction uses the board's built-in capacitive touch screen. When the user taps the character sprite area, the firmware registers it as a touch event and includes `"touched": true` in the next sensor POST. No separate touch sensor module is needed.

---

## 4. Location tracking

### 4.1 How it works

WeWork offices use multiple WiFi access points across floors and zones. Each access point has a unique BSSID (MAC address). When your MacBook connects to WiFi, it connects to whichever access point is closest. By mapping BSSIDs to known locations, we can determine where you are in the office automatically.

### 4.2 Validated location data

Captured at the WeWork office:

| Location | Display label | Signal strength | Channel | Notes |
|----------|--------------|----------------|---------|-------|
| desk | "At desk" | -59 dBm (strong) | 5GHz ch161 | Primary desk, main floor |
| cubicle | "Private cubicle" | -78 dBm (weak) | 5GHz ch56 | Private cubicle, main floor |
| roof | "Rooftop" | -66 dBm (moderate) | 5GHz ch112 | Rooftop area |
| office_floor_cubicle | "Downstairs cubicle" | -75 dBm (weak) | 5GHz ch161 | Cubicle on a different floor |

All four locations have distinct BSSIDs. The 5GHz WiFi band has shorter range per access point, which creates sharp zone boundaries — the BSSID reliably flips as you move between areas.

### 4.3 BSSID daemon

A shell script runs as a macOS LaunchAgent on the MacBook every 30 seconds.

**Config file** (`~/deskbuddy/bssid-map.json`):
```json
{
  "server": "http://deskbuddy.local:3777",
  "locations": {
    "<desk-bssid>": { "id": "desk", "label": "At desk" },
    "<cubicle-bssid>": { "id": "cubicle", "label": "Private cubicle" },
    "<roof-bssid>": { "id": "roof", "label": "Rooftop" },
    "<floor-cubicle-bssid>": { "id": "office_floor_cubicle", "label": "Downstairs cubicle" }
  },
  "default": { "id": "unknown", "label": "Unknown" }
}
```

**Logic:**
1. Read current BSSID via macOS `airport` utility
2. If WiFi is disconnected or BSSID is empty → skip this cycle
3. Look up BSSID in the config map
4. POST `{"location": "<id>", "bssid": "<bssid>"}` to the Mac Mini
5. If BSSID is not in the map → POST with `"location": "unknown"`

**LaunchAgent plist** (`~/Library/LaunchAgents/com.deskbuddy.location.plist`):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.deskbuddy.location</string>
  <key>ProgramArguments</key>
  <array>
    <string>/bin/bash</string>
    <string>/Users/aakash/deskbuddy/location-daemon.sh</string>
  </array>
  <key>StartInterval</key>
  <integer>30</integer>
  <key>RunAtLoad</key>
  <true/>
</dict>
</plist>
```

### 4.4 Server-side location handling

The Mac Mini stores the most recent location in memory:
```javascript
let locationState = {
  location: "unknown",
  label: "Unknown",
  bssid: null,
  lastUpdate: null
};
```

If no location update arrives for 5 minutes, the server marks the user as "away." This handles MacBook sleep, going home, and WiFi disconnection — the "sleeping" character state triggers automatically.

---

## 5. API contract

### 5.1 Endpoints

| Method | Path | Called by | Purpose |
|--------|------|----------|---------|
| GET | /status | ESP32 (every 30s) | Returns resolved display state + all data |
| POST | /sensors | ESP32 (every 10s) | Receives plant sensor readings |
| POST | /location | MacBook daemon (every 30s) | Receives WiFi location |
| POST | /override | Dashboard (manual) | Force a specific display state |
| GET | /dashboard | MacBook browser | Debug and control web UI |
| GET | /health | Any | Server health check |

### 5.2 GET /status

This is the main endpoint. The ESP32 calls it every 30 seconds. It returns a single JSON object containing everything the display needs to render.

```json
{
  "state": "coding",
  "location": "desk",
  "locationLabel": "At desk",
  "task": "RAG architecture",
  "taskKey": "JACK-142",
  "status": "Deep in code",
  "calendar": {
    "inMeeting": false,
    "currentMeeting": null,
    "nextMeeting": "Sprint Planning",
    "nextMeetingIn": 45
  },
  "plant": {
    "moisture": 62,
    "moistureStatus": "good",
    "temperature": 24.1,
    "humidity": 55,
    "light": 820,
    "lightStatus": "bright",
    "touched": false
  },
  "updatedAt": "2026-03-30T12:30:00Z"
}
```

The `state` field is one of exactly 10 values:

`coding` · `meeting` · `rooftop` · `focus` · `idle` · `happy` · `thirsty` · `loved` · `sleeping` · `overwatered`

The ESP32 reads `state` and renders the corresponding sprite. It reads `locationLabel`, `task`, and `status` for the text areas. It reads `plant.*` for the health indicators.

### 5.3 POST /sensors

```json
{
  "moisture": 62,
  "temperature": 24.1,
  "humidity": 55,
  "light": 820,
  "touched": false
}
```

**Sensor thresholds:**

| Sensor | Range | Thresholds |
|--------|-------|-----------|
| Moisture | 0-100% | < 20 = dry, 20-80 = good, > 90 = soggy |
| Temperature | °C | Displayed as-is |
| Humidity | % | Displayed as-is |
| Light | Lux | < 50 = dark, 50-300 = dim, > 300 = bright |
| Touch | Boolean | true = screen tapped on character area |

### 5.4 POST /location

```json
{
  "location": "desk",
  "bssid": "aa:bb:cc:11:22:33"
}
```

Valid location values: `desk`, `cubicle`, `roof`, `office_floor_cubicle`, `unknown`

### 5.5 POST /override

```json
{
  "state": "focus",
  "expiresIn": 1800
}
```

Manually forces a display state. Expires after the specified seconds (default 1800 = 30 minutes) or when a higher-priority trigger fires.

### 5.6 GET /health

```json
{
  "status": "healthy",
  "uptime": 86400,
  "lastSensorUpdate": "2026-03-30T12:30:05Z",
  "lastLocationUpdate": "2026-03-30T12:30:12Z",
  "calendarConnected": true,
  "jiraConnected": true
}
```

---

## 6. State resolution engine

### 6.1 Priority cascade

Every time the ESP32 requests `GET /status`, the server evaluates conditions top-to-bottom. The first match wins and becomes the `state` field in the response.

```
Priority 1: Plant emergency
    moisture < 20              → "thirsty"
    moisture > 90              → "overwatered"

Priority 2: Manual override
    override.active = true     → override.state

Priority 3: Touch sensor
    touched = true             → "loved" (hold for 10 seconds)

Priority 4: Darkness
    light < 50 lux             → "sleeping"

Priority 5: User away
    lastLocationUpdate > 5min  → "sleeping"

Priority 6: Calendar event
    inMeeting = true           → "meeting"

Priority 7: Location
    location = "roof"                    → "rooftop"
    location = "cubicle"                 → "focus"
    location = "office_floor_cubicle"    → "focus"

Priority 8: Jira activity
    hasInProgressTask = true   → "coding"
    hasInProgressTask = false  → "idle"

Priority 9: Fallback
    (none of the above)        → "happy"
```

### 6.2 Why this order

Plant emergencies are highest because the plant cannot advocate for itself. If soil moisture drops below 20%, the display must show the thirsty state regardless of work context — a teammate might water the plant.

Calendar beats location because you can be in a meeting at your desk. The "meeting" state is more informative to teammates than "at desk."

Location beats Jira because being on the rooftop is a stronger availability signal than whatever task Jira says you're working on.

### 6.3 State-to-display mapping

| State | Body color | Leaf | Eyes | Accessories | Effects | Animation |
|-------|-----------|------|------|-------------|---------|-----------|
| coding | Light green | Healthy, swaying | Focused, narrowed | Headphones | Laptop glow | Float 2s |
| meeting | Light green | Healthy, swaying | Wide, attentive | — | Speech bubble | Float 2.5s |
| rooftop | Light green | Healthy, swaying | Half-closed chill | Sunglasses | Wind lines | Gentle bob 3s |
| focus | Light green | Alert, straight | Intense, glowing pupils | Headphones | Energy aura | Float 1.5s |
| idle | Light green | Healthy, swaying | Relaxed arcs | — | Sparkles | Gentle bob 3.5s |
| happy | Light green | Blooming double | Happy arcs | — | Rosy cheeks | Float 1.8s |
| thirsty | Gray | Drooping, wilted | Sad, teary | — | Tear drip | Shiver |
| loved | Warm green | Blooming double | Heart-shaped | — | Hearts floating | Float 1.8s |
| sleeping | Dark gray | Drooping | Closed lines | — | ZZZ bubbles | Squish breathe 4s |
| overwatered | Blue | Soggy, drooping | Dizzy X's | — | Water drips | Float 2.5s |

The ghost body silhouette is identical in every state. Mood is conveyed through body color tint, eye expression, leaf style, accessories, ambient effects, and animation speed. Each state should be recognizable from 1 meter away without reading text.

### 6.4 Status text per state

| State | Location text | Task text | Status text |
|-------|-------------|-----------|-------------|
| coding | From BSSID | From Jira | "Deep in code" |
| meeting | From BSSID | From Calendar event | "In a meeting" |
| rooftop | "Rooftop" | From Jira or "Thinking time" | "Taking a breather" |
| focus | "Private cubicle" or "Downstairs" | From Jira | "Do not disturb" |
| idle | From BSSID | "Nothing right now" | "Available" |
| happy | From BSSID | From Jira | "Plant is thriving!" |
| thirsty | From BSSID | From Jira | "Plant needs water!" |
| loved | From BSSID | From Jira | "Someone petted me!" |
| sleeping | "Away" | "Gone for the day" | "See you tomorrow" |
| overwatered | From BSSID | From Jira | "Too much water!" |

---

## 7. Character and sprite system

### 7.1 Sprite specifications

| Property | Value |
|----------|-------|
| Canvas size | 48×48 pixels |
| Color depth | 16-bit RGB565 |
| Colors per sprite | 2-3 (minimal pixel art palette) |
| Bytes per frame | 4,608 (48 × 48 × 2) |
| Frames per state | 3-6 |
| Total states | 10 |
| Estimated total frames | ~40 |
| Total sprite memory | ~180 KB |
| Board flash available | 16 MB |
| Memory usage | ~1.1% |

At 3× scaling, each 48×48 sprite renders as 144×144 pixels on the AMOLED. This fills about 39% of the screen width — large enough to be the visual focal point while leaving room for status text.

### 7.2 Color palette

| Color | Hex | Usage |
|-------|-----|-------|
| Main green | #4ECB71 | Default body |
| Light green | #B8E6C8 | Highlights, happy body |
| Dark green | #1A3A24 | Outlines, eyes, mouth |
| Eye white | #E8F5EC | Eye highlight dots |
| Gray | #7A8A7E | Thirsty / sleeping body |
| Blue | #8ABCE6 | Overwatered body |
| Warm green | #C8ECD4 | Loved body |
| Pink | #E6719A | Hearts, rosy cheeks |
| Amber | #E6C84E | Sparkles |
| Dark gray | #5A6A62 | Sleeping body |

### 7.3 Design workflow

1. **Design** in Piskel (piskelapp.com, free, browser-based)
   - 48×48 canvas, draw each state's key frame, add 2-5 animation frames
   - Export as PNG sprite sheet (horizontal strip)

2. **Convert** to firmware format
   - Python script reads PNG, converts each pixel to RGB565
   - Outputs C header files: `const uint16_t sprite_coding[] PROGMEM = { ... };`

3. **Compile** into ESP32 firmware
   - Sprite state machine maps the `state` string from /status to the correct sprite array
   - LVGL animation timer cycles through frames at the appropriate speed per state

### 7.4 Rendering

The Waveshare board uses QSPI for the display (SH8601 driver). LVGL is the recommended UI framework — it handles double buffering, anti-tearing, touch input, and animation natively. Waveshare provides working LVGL example code for this board.

---

## 8. Google Calendar integration

### 8.1 Authentication

OAuth2 with an offline refresh token. One-time setup:

1. Create a Google Cloud project, enable Calendar API
2. Create OAuth2 credentials (Desktop Application type)
3. Download credentials JSON to the Mac Mini
4. Run a one-time auth script that opens a browser for consent
5. Store the refresh token in `.env`

The server uses the refresh token to automatically obtain short-lived access tokens. No manual intervention after initial setup.

### 8.2 Polling

Every 60 seconds, the server fetches events for the next 2 hours:

```
GET https://www.googleapis.com/calendar/v3/calendars/primary/events
  ?timeMin={now}
  &timeMax={now + 2h}
  &singleEvents=true
  &orderBy=startTime
  &maxResults=5
```

The response is processed to extract:
- `inMeeting`: is there an event happening right now?
- `currentMeeting`: the title of the current event
- `nextMeeting`: the title of the next upcoming event
- `nextMeetingIn`: minutes until the next event

Results are cached for 60 seconds.

---

## 9. Jira integration

### 9.1 Authentication

API token + email, sent as a Basic auth header. Generate a token at `id.atlassian.com/manage/api-tokens`.

### 9.2 Polling

Every 60 seconds, the server fetches the user's current in-progress task from the active sprint:

```
GET https://{domain}.atlassian.net/rest/api/3/search
  ?jql=assignee=currentUser() AND sprint in openSprints() AND status="In Progress"
  &fields=summary,status,key
  &maxResults=1
```

Extracts:
- `hasInProgressTask`: are there any in-progress tasks?
- `currentTask`: the issue key (e.g., "JACK-142")
- `summary`: the task title
- `status`: the Jira status name

Results are cached for 60 seconds.

---

## 10. ESP32 firmware

### 10.1 Libraries

| Library | Purpose |
|---------|---------|
| LVGL | UI framework — display rendering, touch handling, animations, layout |
| ArduinoJson | JSON parsing for /status responses |
| WiFi.h | WiFi connection management |
| HTTPClient.h | HTTP GET/POST to Mac Mini |
| Wire.h | I2C for external BME280 + BH1750 |
| Adafruit_BME280 | Temperature / humidity sensor driver |
| BH1750.h | Light sensor driver |
| ESPmDNS.h | mDNS hostname resolution |
| Waveshare board drivers | SH8601 display, FT3168 touch, AXP2101 power |

ESP-IDF is the recommended development framework for best LVGL performance with double buffering and DMA. Arduino IDE is also supported but with reduced display refresh rates.

### 10.2 Main loop

```
setup():
  initDisplay()           // QSPI AMOLED via LVGL
  initTouch()             // FT3168 capacitive touch via I2C
  initExternalSensors()   // I2C for BME280 + BH1750, analog for moisture
  connectWiFi()           // WeWork WiFi
  resolveServer()         // mDNS: deskbuddy.local → IP
  showBootScreen()        // "DeskBuddy starting..." on AMOLED

loop():
  every 10 seconds:
    read moisture (analog)
    read BME280 (temp, humidity)
    read BH1750 (light)
    check touch screen for tap on character area
    POST all readings to /sensors

  every 30 seconds:
    GET /status from Mac Mini
    parse JSON
    if state changed → load new sprite set, trigger transition animation
    update status text (location, task, status line)
    update plant health indicators

  continuously:
    cycle animation frames at state-specific speed (200-500ms per frame)

  on WiFi disconnect:
    reconnect with exponential backoff

  if server unreachable for 5 minutes:
    show "offline" state (sleeping character + "No connection" text)
```

### 10.3 Display layout

```
┌──────────────────────────────────────────────┐
│                368 × 448 AMOLED              │
│                                              │
│  ┌──────────────┐  ┌──────────────────────┐  │
│  │              │  │                      │  │
│  │  CHARACTER   │  │  LOCATION            │  │
│  │  SPRITE      │  │  At desk             │  │
│  │              │  │                      │  │
│  │  144 × 144   │  │  WORKING ON          │  │
│  │  rendered    │  │  RAG architecture    │  │
│  │              │  │                      │  │
│  │  (48×48      │  │  STATUS              │  │
│  │   scaled 3×) │  │  Deep in code        │  │
│  │              │  │                      │  │
│  └──────────────┘  └──────────────────────┘  │
│                                              │
│  ┌──────────────────────────────────────┐    │
│  │  ● Moist  ● 24°C  ● 55%  ● Bright  │    │
│  └──────────────────────────────────────┘    │
│                                              │
│  ┌──────────────────────────────────────┐    │
│  │      Next: Sprint Planning (45min)   │    │
│  └──────────────────────────────────────┘    │
│                                              │
└──────────────────────────────────────────────┘

Character area: x=10, y=20, w=160, h=180
Status area:    x=178, y=20, w=180, h=180
Plant bar:      x=10, y=220, w=348, h=30
Info bar:       x=10, y=260, w=348, h=30
```

The AMOLED's true blacks make unused areas disappear, so the layout feels spacious despite the small screen. The portrait orientation gives more vertical room than a typical landscape LCD.

Touch: tapping the character area triggers the "loved" state. Future: swipe gestures to cycle between info views.

---

## 11. Wiring

### 11.1 What's already wired inside the board

Everything that comes on the Waveshare board is internally connected — no wiring needed:

- AMOLED display (SH8601, QSPI)
- Capacitive touch (FT3168, I2C)
- 6-axis IMU (QMI8658, I2C)
- RTC (PCF85063, I2C)
- Audio codec (ES8311, I2S) + mic + speaker
- Power management (AXP2101, I2C)

### 11.2 External sensor connections

Three sensors connect to the board's exposed GPIO header:

| Sensor | Wire to | Notes |
|--------|---------|-------|
| BME280 SDA | Board I2C SDA pad | Shared I2C bus |
| BME280 SCL | Board I2C SCL pad | Shared I2C bus |
| BH1750 SDA | Board I2C SDA pad | Same bus, different address |
| BH1750 SCL | Board I2C SCL pad | Same bus |
| Moisture signal | Available analog GPIO | Check Waveshare wiki for exact pin number |
| All VCC | 3.3V from header | |
| All GND | GND from header | |

The board's internal I2C bus (for IMU, RTC, touch) uses separate pins from the external I2C pads, so there's no conflict.

---

## 12. Desk setup

The Waveshare board ships with its own compact case. For the initial build:

- The board in its stock case sits next to the plant pot on the desk
- The moisture sensor probe runs from the board's GPIO header into the soil
- The BME280 and BH1750 attach near the board or sit beside the plant with short wires
- A single USB-C cable powers the board
- The Mac Mini sits elsewhere on the desk, connected to the same WiFi

```
    ┌──────────┐     ┌─────────────┐
    │  Plant   │     │  Waveshare  │
    │  pot     │     │  AMOLED     │
    │          │     │  board      │
    │  ┌probe┐ │     │  (in stock  │
    │  │wire ─│─┼─────│→ GPIO      │
    │  └─────┘ │     │  header     │
    └──────────┘     └──────┬──────┘
                            │
                         USB-C
                        (power)

    BME280 + BH1750 near the board
    Mac Mini elsewhere on desk
```

A custom 3D-printed integrated enclosure (combining board cradle + plant pot + sensor housing) is a future option but not needed to ship v1.

---

## 13. Server deployment

### 13.1 Setup

The Mac Mini runs macOS with Node.js 20 LTS (install via Homebrew). The server auto-starts on boot via a launchd plist.

**launchd plist** (`~/Library/LaunchAgents/com.deskbuddy.server.plist`):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.deskbuddy.server</string>
  <key>ProgramArguments</key>
  <array>
    <string>/usr/local/bin/node</string>
    <string>/Users/aakash/deskbuddy/server.js</string>
  </array>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>WorkingDirectory</key>
  <string>/Users/aakash/deskbuddy</string>
  <key>EnvironmentVariables</key>
  <dict>
    <key>NODE_ENV</key>
    <string>production</string>
  </dict>
  <key>StandardOutPath</key>
  <string>/Users/aakash/deskbuddy/logs/stdout.log</string>
  <key>StandardErrorPath</key>
  <string>/Users/aakash/deskbuddy/logs/stderr.log</string>
</dict>
</plist>
```

### 13.2 Environment variables

```env
PORT=3777
NODE_ENV=production

# Google Calendar
GOOGLE_CLIENT_ID=
GOOGLE_CLIENT_SECRET=
GOOGLE_REFRESH_TOKEN=

# Jira
JIRA_DOMAIN=yourteam.atlassian.net
JIRA_EMAIL=
JIRA_TOKEN=

# Sensor thresholds
MOISTURE_DRY=20
MOISTURE_SOGGY=90
LIGHT_DARK=50
LIGHT_DIM=300

# Timeouts (seconds)
LOCATION_TIMEOUT=300
TOUCH_HOLD=10
OVERRIDE_DEFAULT_EXPIRY=1800
```

### 13.3 Maintenance

- **Update server code:** `cd ~/deskbuddy && git pull && pm2 restart deskbuddy`
- **Update sprites:** regenerate C headers from Piskel exports, reflash ESP32 via USB
- **Add new locations:** edit `bssid-map.json` on the MacBook — no server restart needed
- **Rotate API tokens:** update `.env` and restart the server

---

## 14. Development plan (9 days)

### Day 1 — Server scaffold + board test

**Software (Aakash):**
- Install Node.js on Mac Mini, set up mDNS (`deskbuddy.local`)
- Scaffold Express server with `GET /status` returning hardcoded JSON
- Verify endpoint from MacBook browser

**Hardware (HW team):**
- Flash Waveshare example firmware, confirm AMOLED display + touch work
- Connect board to WeWork WiFi
- Test HTTP GET from board to Mac Mini endpoint

**Milestone:** Board fetches JSON from Mac Mini. Display and touch confirmed working.

### Day 2 — Sprites + display layout

**Software (Aakash):**
- Design base ghost sprite in Piskel (48×48, 3 colors)
- Design all 10 state variations
- Build PNG → RGB565 converter, generate C headers

**Hardware (HW team):**
- Set up LVGL with Waveshare board drivers (SH8601 + FT3168)
- Build sprite rendering via LVGL image widget
- Implement display layout (character, status text, bars)

**Milestone:** Ghost character visible on the AMOLED.

### Day 3 — Plant sensors + state engine

**Software (Aakash):**
- Build `POST /sensors` endpoint
- Implement state priority cascade
- Wire into `GET /status`
- Test with mock data via curl

**Hardware (HW team):**
- Connect moisture sensor, BME280, BH1750 to board GPIO header
- Implement touch-screen tap detection (replaces separate touch sensor)
- Build sensor read loop: all sensors + touch → POST every 10s

**Milestone:** Real sensor data → Mac Mini → resolved state → correct sprite on AMOLED.

### Day 4 — Animations + transitions

**Software (Aakash):**
- Add 2-4 animation frames per state in Piskel
- Generate updated C headers

**Hardware (HW team):**
- Implement sprite state machine (state string → sprite set)
- Build animation frame loop with per-state timing
- Render status text and plant health indicators
- Handle state transitions with fade/slide effects

**Milestone:** Character animates, transitions smoothly between states, shows live text.

### Day 5 — Google Calendar + Jira

**Software (Aakash):**
- Google Cloud project setup, OAuth2 credentials, one-time auth flow
- Build calendar polling module (60s interval)
- Jira API token setup
- Build Jira polling module (60s interval)
- Wire both into state engine
- Add 60s caching layer

**Milestone:** Mac Mini knows calendar and current Jira task. States resolve correctly.

### Day 6 — WiFi location tracking

**Software (Aakash):**
- Build BSSID daemon with the 4-location map
- Create macOS LaunchAgent
- Build `POST /location` endpoint
- Add 5-minute away timeout
- Wire location into state engine
- Test: walk to rooftop → character puts on sunglasses

**Milestone:** Automatic location tracking via MacBook WiFi.

### Day 7 — Dashboard + manual override

**Software (Aakash):**
- Build `/dashboard` web UI served from Mac Mini
- Show live state, sensor readings, location
- Manual override buttons for all 10 states
- Build `POST /override` with expiry timer
- Sensor history chart (last 24h)
- Location change log
- Mobile-responsive layout

**Milestone:** Full control panel at `deskbuddy.local:3777/dashboard`.

### Day 8 — Reliability

**Software (Aakash):**
- Auto-start server on Mac Mini boot (launchd plist)
- ESP32 WiFi reconnect with exponential backoff
- Graceful handling of Calendar/Jira API failures
- `GET /health` endpoint
- BSSID daemon: handle MacBook wake from sleep
- Overnight test: sensors log, sleeping state triggers in dark

**Hardware (HW team):**
- Finalize sensor wiring (clean permanent connections)
- Verify board stability under continuous operation

**Milestone:** System survives overnight, Mac Mini reboot, WiFi drop, and MacBook sleep.

### Day 9 — Deploy

**Software (Aakash):**
- End-to-end test of all 10 states
- Walk through all 4 locations, verify transitions
- Write project README
- Deploy BSSID LaunchAgent permanently

**Hardware (HW team):**
- Solder sensor wires permanently
- Secure BME280 + BH1750 near the board
- Route moisture probe neatly into plant pot
- Position board next to plant on desk

**Together:**
- Verify teammates can see the display
- Ship it

---

## 15. Future enhancements

These are not part of the initial build. They're documented here so future developers know what's possible with the existing hardware and infrastructure.

- **Claude AI rubber duck** — add Anthropic Claude API to the server for a built-in coding assistant, accessible via a web chat UI on the MacBook. The display shows "thinking" / "talking" character states while Claude processes.
- **Voice interaction** — the Waveshare board already has an onboard mic and speaker. Add speech-to-text (Whisper) for voice input and TTS for Claude responses. No hardware changes needed.
- **Slack status sync** — automatically update Slack status and emoji based on DeskBuddy state
- **GitHub integration** — show open PR count, CI/CD build status, review requests
- **Multiple DeskBuddies** — teammates get their own devices, characters can interact on the display
- **Historical dashboard** — long-term plant health trends, work pattern analytics
- **Motion detection** — use the onboard IMU to detect if someone picks up or moves the device
- **Portable mode** — the board supports a 3.7V LiPo battery; could enable untethered operation
- **TF card storage** — use the onboard SD slot to store sprite assets or log sensor data locally
- **OTA firmware updates** — push ESP32 firmware wirelessly via the Mac Mini
- **Custom enclosure** — 3D-printed integrated unit combining board cradle + plant pot + sensor housing
