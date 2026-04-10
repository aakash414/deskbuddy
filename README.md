# DeskBuddy

A desk companion that shows your plant's health and your work presence. A pixel-art plant-ghost mascot on a 1.8" AMOLED screen reacts to soil moisture, ambient light, your office location, Jira tasks, and Google Calendar meetings. Teammates glance at it to see where you are — without interrupting on Teams
.

## Architecture

```
MacBook (moves with you)           Raspberry Pi (always on at desk)
┌─────────────────────┐            ┌──────────────────────────┐
│ BSSID location      │──POST /loc─▶│ Node.js server :3777     │
│ daemon (every 30s)  │            │ Calendar + Jira +         │
│                     │            │ state engine              │
└─────────────────────┘            └────────────┬─────────────┘
                                                │
                                     GET /status│ POST /sensors
                                                │
                                   ┌────────────▼─────────────┐
                                   │ Waveshare ESP32-S3       │
                                   │ Touch-AMOLED-1.8         │
                                   │ + soil moisture + BH1750 │
                                   └──────────────────────────┘
```

## Project Structure

```
server/          Node.js + Express API server (runs on Raspberry Pi)
firmware/        ESP32 firmware (runs on Waveshare board)
scripts/         BSSID location daemon, sprite converter, utilities
docs/            Specs, memory, pinout reference
```

## Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.8
- Capacitive Soil Moisture Sensor v2.0 (GPIO17, ADC2)
- GY-302 BH1750 Light Sensor (I2C, 0x23)
- Raspberry Pi (any model, server)

## Quick Start

### Server (Raspberry Pi)

```bash
# Install dependencies (one-time)
sudo apt install nodejs avahi-daemon

# Clone repo and configure
cd server
npm install
cp .env.example .env   # fill in your API keys

# Install and start as a system service
sudo cp ../scripts/deskbuddy-server.service /etc/systemd/system/
sudo systemctl enable deskbuddy-server
sudo systemctl start deskbuddy-server
```

The server will be reachable at `http://deskbuddy.local:3777` on the local network (avahi-daemon handles the `.local` hostname). Adjust `User=pi` in the service file if your Pi OS username is different.

### Location Daemon (MacBook)

```bash
cp scripts/bssid-map.example.json ~/deskbuddy/bssid-map.json
# edit with your WeWork BSSIDs
cp scripts/com.deskbuddy.location.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.deskbuddy.location.plist
```

### Firmware (ESP32)

See `firmware/README.md` for flashing instructions.

## License

MIT
