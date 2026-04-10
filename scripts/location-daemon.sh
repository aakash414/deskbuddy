#!/bin/bash
# DeskBuddy location daemon
# Reads WiFi router MAC (via ARP), maps to office location, POSTs to server
# Uses router MAC instead of BSSID because macOS 26+ redacts BSSID everywhere
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

# Get default gateway IP, then resolve its MAC address via ARP table.
# macOS 26+ redacts BSSID everywhere (wdutil, system_profiler, CoreWLAN without
# Location Services). Router MAC from ARP is not redacted and uniquely identifies
# the network — good enough for single-AP setups.
GATEWAY=$(route -n get default 2>/dev/null | awk '/gateway:/{print $2}')
if [ -z "$GATEWAY" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) SKIP: no default gateway (no network?)" >> "$LOG"
  exit 0
fi

# arp output format: "? (10.x.x.x) at 0:9:f:9:a:6 on en0 ..."
# Normalize to zero-padded colon-separated MAC (aa:bb:cc:dd:ee:ff)
RAW_MAC=$(arp -n "$GATEWAY" 2>/dev/null | awk '{print $4}')
if [ -z "$RAW_MAC" ] || [ "$RAW_MAC" = "(incomplete)" ]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) SKIP: could not resolve gateway MAC (WiFi disconnected?)" >> "$LOG"
  exit 0
fi

# Zero-pad each octet: 0:9:f:9:a:6 → 00:09:0f:09:0a:06
BSSID=$(echo "$RAW_MAC" | python3 -c "import sys; print(':'.join(f'{int(x,16):02x}' for x in sys.stdin.read().strip().split(':')))")

# Validate MAC format
if ! [[ "$BSSID" =~ ^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$ ]]; then
  echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) SKIP: invalid MAC format: '$BSSID'" >> "$LOG"
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
