#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "config.h"

struct StatusData {
    String state;
    String locationLabel;
    String alternatingText[2];
    int altTextCount;
    float moisture;
    String moistureStatus;
    float light;
    String lightStatus;
    bool valid;
};

class Network {
public:
    bool connected = false;
    String serverUrl;
    unsigned long lastSuccessfulFetch = 0;

    void begin() {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.printf("Connecting to %s", WIFI_SSID);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
            resolveServer();
        } else {
            Serial.println("\nWiFi connection failed");
        }
    }

    void maintain() {
        if (WiFi.status() != WL_CONNECTED) {
            connected = false;
            reconnect();
        }
    }

    bool isOffline() {
        if (lastSuccessfulFetch == 0) return true;
        return (millis() - lastSuccessfulFetch) > OFFLINE_TIMEOUT;
    }

    StatusData fetchStatus() {
        StatusData data;
        data.valid = false;
        data.altTextCount = 0;

        if (!connected || serverUrl.isEmpty()) return data;

        HTTPClient http;
        http.begin(serverUrl + "/status");
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String body = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);

            if (!err) {
                data.state = doc["state"].as<String>();
                data.locationLabel = doc["locationLabel"].as<String>();

                JsonArray alt = doc["alternatingText"].as<JsonArray>();
                int i = 0;
                for (JsonVariant v : alt) {
                    if (i < 2) {
                        data.alternatingText[i] = v.as<String>();
                        i++;
                    }
                }
                data.altTextCount = i;

                data.moisture = doc["plant"]["moisture"] | -1.0f;
                data.moistureStatus = doc["plant"]["moistureStatus"].as<String>();
                data.light = doc["plant"]["light"] | -1.0f;
                data.lightStatus = doc["plant"]["lightStatus"].as<String>();

                data.valid = true;
                lastSuccessfulFetch = millis();
            } else {
                Serial.printf("JSON parse error: %s\n", err.c_str());
            }
        } else {
            Serial.printf("GET /status failed: %d\n", code);
        }

        http.end();
        return data;
    }

    bool postSensors(float moisture, float light, bool touched) {
        if (!connected || serverUrl.isEmpty()) return false;

        HTTPClient http;
        http.begin(serverUrl + "/sensors");
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);

        JsonDocument doc;
        doc["moisture"] = moisture;
        doc["light"] = light;
        doc["touched"] = touched;

        String body;
        serializeJson(doc, body);

        int code = http.POST(body);
        http.end();

        if (code == 200) {
            return true;
        }
        Serial.printf("POST /sensors failed: %d\n", code);
        return false;
    }

private:
    unsigned long lastReconnect = 0;
    int reconnectDelay = 1000;

    void resolveServer() {
        // Try mDNS first
        if (MDNS.begin("deskbuddy-display")) {
            IPAddress ip = MDNS.queryHost(SERVER_HOST);
            if (ip != IPAddress(0, 0, 0, 0)) {
                serverUrl = "http://" + ip.toString() + ":" + String(SERVER_PORT);
                Serial.printf("Resolved %s -> %s\n", SERVER_HOST, serverUrl.c_str());
                return;
            }
        }

        // Fallback to hardcoded IP
        serverUrl = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT);
        Serial.printf("mDNS failed, using fallback: %s\n", serverUrl.c_str());
    }

    void reconnect() {
        if (millis() - lastReconnect < reconnectDelay) return;

        Serial.println("WiFi reconnecting...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        lastReconnect = millis();

        // Exponential backoff: 1s, 2s, 4s, 8s, max 30s
        reconnectDelay = min(reconnectDelay * 2, 30000);

        delay(3000);
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            reconnectDelay = 1000;
            Serial.println("Reconnected!");
            resolveServer();
        }
    }
};
