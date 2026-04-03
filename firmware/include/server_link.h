#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "shared_state.h"

// HTTP client for deskbuddy server. WiFi connection is managed by WiFiManager.
class ServerLink {
public:
    String serverUrl;
    unsigned long lastSuccessfulFetch = 0;

    void begin(const String& url, const String& apiKey) {
        serverUrl = url;
        _apiKey   = apiKey;
    }

    bool isOffline() {
        if (lastSuccessfulFetch == 0) return true;
        return (millis() - lastSuccessfulFetch) > OFFLINE_TIMEOUT;
    }

    StatusData fetchStatus() {
        StatusData data;
        if (WiFi.status() != WL_CONNECTED || serverUrl.isEmpty()) return data;

        HTTPClient http;
        http.begin(serverUrl + "/status");
        http.addHeader("x-api-key", _apiKey);
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String body = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);

            if (!err) {
                data.state        = doc["state"].as<String>();
                data.locationLabel = doc["locationLabel"].as<String>();

                JsonArray alt = doc["alternatingText"].as<JsonArray>();
                int i = 0;
                for (JsonVariant v : alt) {
                    if (i < 2) data.alternatingText[i++] = v.as<String>();
                }
                data.altTextCount = i;

                data.moisture      = doc["plant"]["moisture"] | -1.0f;
                data.moistureStatus = doc["plant"]["moistureStatus"].as<String>();
                data.light         = doc["plant"]["light"] | -1.0f;
                data.lightStatus   = doc["plant"]["lightStatus"].as<String>();

                data.valid = true;
                lastSuccessfulFetch = millis();
            } else {
                Serial.printf("[net] JSON parse error: %s\n", err.c_str());
            }
        } else {
            Serial.printf("[net] GET /status failed: %d\n", code);
        }

        http.end();
        return data;
    }

    bool postSensors(float moisture, float light, bool touched) {
        if (WiFi.status() != WL_CONNECTED || serverUrl.isEmpty()) return false;

        HTTPClient http;
        http.begin(serverUrl + "/sensors");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-api-key", _apiKey);
        http.setTimeout(5000);

        JsonDocument doc;
        doc["moisture"] = moisture;
        doc["light"]    = light;
        doc["touched"]  = touched;

        String body;
        serializeJson(doc, body);
        int code = http.POST(body);
        http.end();

        if (code == 200) return true;
        Serial.printf("[net] POST /sensors failed: %d\n", code);
        return false;
    }

private:
    String _apiKey;
};
