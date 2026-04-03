#!/bin/bash
# DeskBuddy location daemon
# Reads WiFi BSSID, maps to office location, POSTs to server
# Run via macOS LaunchAgent every 30 seconds

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="${DESKBUDDY_CONFIG:-$HOME/deskbuddy/bssid-map.json}"
LOG="${DESKBUDDY_LOG:-$HOME/deskbuddy/location.log}"

if [ ! -f "$CONFIG" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) ERROR: config not found at $CONFIG" >> "$LOG"
  exit 1
fi

SERVER=$(python3 -c "import json; print(json.load(open('$CONFIG'))['server'])" 2>/dev/null)
if [ -z "$SERVER" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) ERROR: could not read server from config" >> "$LOG"
  exit 1
fi

AIRPORT="/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport"

if [ -x "$AIRPORT" ]; then
  BSSID=$("$AIRPORT" -I 2>/dev/null | awk '/ BSSID:/{print $2}')
else
  BSSID=$(system_profiler SPAirPortDataType 2>/dev/null | awk '/BSSID:/{print $2}' | head -1)
fi

if [ -z "$BSSID" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) SKIP: no BSSID (WiFi disconnected?)" >> "$LOG"
  exit 0
fi

LOCATION=$(python3 -c "
import json, sys
cfg = json.load(open('$CONFIG'))
bssid = '$BSSID'.lower()
locations = cfg.get('locations', {})
for key, val in locations.items():
    if key.lower() == bssid:
        print(val['id'])
        sys.exit(0)
print(cfg.get('default', {}).get('id', 'unknown'))
" 2>/dev/null)

if [ -z "$LOCATION" ]; then
  LOCATION="unknown"
fi

RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" \
  -X POST "$SERVER/location" \
  -H "Content-Type: application/json" \
  -d "{\"location\": \"$LOCATION\", \"bssid\": \"$BSSID\"}" \
  --connect-timeout 5 \
  --max-time 10)

echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) bssid=$BSSID location=$LOCATION http=$RESPONSE" >> "$LOG"
