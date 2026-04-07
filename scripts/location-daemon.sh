#!/bin/bash
# DeskBuddy location daemon
# Reads WiFi BSSID, maps to office location, POSTs to server
# Run via macOS LaunchAgent every 30 seconds

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="${DESKBUDDY_CONFIG:-$HOME/deskbuddy/bssid-map.json}"
LOG="${DESKBUDDY_LOG:-$HOME/deskbuddy/location.log}"

# Ensure log directory exists
mkdir -p "$(dirname "$LOG")"

if [ ! -f "$CONFIG" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) ERROR: config not found at $CONFIG" >> "$LOG"
  exit 1
fi

# Pass CONFIG as an argument to avoid shell injection via variable embedding
SERVER=$(python3 -c "
import json, sys
try:
    print(json.load(open(sys.argv[1]))['server'])
except Exception as e:
    print('ERROR: ' + str(e), file=sys.stderr)
    sys.exit(1)
" "$CONFIG" 2>>"$LOG")
if [ $? -ne 0 ] || [ -z "$SERVER" ]; then
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

# Validate BSSID format (xx:xx:xx:xx:xx:xx) to catch garbled output
if ! [[ "$BSSID" =~ ^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$ ]]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) SKIP: invalid BSSID format: '$BSSID'" >> "$LOG"
  exit 0
fi

# Pass CONFIG and BSSID as arguments to avoid shell injection
LOCATION=$(python3 -c "
import json, sys
cfg = json.load(open(sys.argv[1]))
bssid = sys.argv[2].lower()
locations = cfg.get('locations', {})
for key, val in locations.items():
    if key.lower() == bssid:
        print(val['id'])
        sys.exit(0)
print(cfg.get('default', {}).get('id', 'unknown'))
" "$CONFIG" "$BSSID" 2>/dev/null)

if [ -z "$LOCATION" ]; then
  LOCATION="unknown"
fi

# Pass CONFIG as argument to avoid shell injection
API_KEY=$(python3 -c "
import json, sys
print(json.load(open(sys.argv[1])).get('apiKey', ''))
" "$CONFIG" 2>/dev/null)

RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" \
  -X POST "$SERVER/location" \
  -H "Content-Type: application/json" \
  -H "x-api-key: $API_KEY" \
  --data-binary "{\"location\": \"$LOCATION\", \"bssid\": \"$BSSID\"}" \
  --connect-timeout 5 \
  --max-time 10 \
  --retry 2 \
  --retry-delay 1 \
  --retry-connrefused)

if [ "$RESPONSE" != "200" ] && [ "$RESPONSE" != "201" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) WARN: server returned HTTP $RESPONSE (bssid=$BSSID location=$LOCATION)" >> "$LOG"
else
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) bssid=$BSSID location=$LOCATION http=$RESPONSE" >> "$LOG"
fi
