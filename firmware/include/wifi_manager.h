#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "config.h"
#include "shared_state.h"

class WiFiManager {
public:
    void begin() {
        // Load credentials from NVS, fall back to config.h defaults
        Preferences prefs;
        prefs.begin("deskbuddy", false);
        _ssid     = prefs.isKey("ssid")        ? prefs.getString("ssid")                      : WIFI_SSID;
        _password = prefs.isKey("password")    ? prefs.getString("password")                  : WIFI_PASSWORD;
        _serverIp = prefs.isKey("server_ip")   ? prefs.getString("server_ip")                 : SERVER_IP;
        _port     = prefs.isKey("server_port") ? (uint16_t)prefs.getUInt("server_port")       : (uint16_t)SERVER_PORT;
        _apiKey   = prefs.isKey("api_key")     ? prefs.getString("api_key")                   : API_KEY;
        prefs.end();

        Serial.printf("[WiFi] Connecting to '%s'\n", _ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(_ssid.c_str(), _password.c_str());

        for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (WiFi.status() == WL_CONNECTED) {
            esp_wifi_set_ps(WIFI_PS_NONE);
            lockState();
            gWifiConnected = true;
            gDeviceIP = WiFi.localIP().toString();
            unlockState();
            _serverUrl = "http://" + _serverIp + ":" + String(_port);
            Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] Server: %s\n", _serverUrl.c_str());
            if (MDNS.begin("deskbuddy")) {
                MDNS.addService("http", "tcp", 80);
                Serial.println("[mDNS] http://deskbuddy.local registered");
            }
        } else {
            Serial.printf("[WiFi] Failed (status=%d) — staying offline, will retry\n", (int)WiFi.status());
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
        }
    }

    bool   isConnected()  { return gWifiConnected; }
    String getServerUrl() { return _serverUrl; }
    String getApiKey()    { return _apiKey; }

    // Attempt to reconnect to the STA network. Returns true on success.
    bool tryReconnect() {
        if (gWifiConnected) return true;
        if (_ssid.isEmpty()) return false;

        Serial.printf("[WiFi] Attempting STA reconnect to '%s'\n", _ssid.c_str());
        // Reinitialize cleanly — WiFi may be in WIFI_OFF from a previous failure
        WiFi.mode(WIFI_STA);
        vTaskDelay(pdMS_TO_TICKS(200));
        WiFi.begin(_ssid.c_str(), _password.c_str());

        for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Reconnect failed, will retry later");
            return false;
        }

        esp_wifi_set_ps(WIFI_PS_NONE);
        lockState();
        gWifiConnected = true;
        gDeviceIP = WiFi.localIP().toString();
        unlockState();
        _serverUrl = "http://" + _serverIp + ":" + String(_port);
        Serial.printf("[WiFi] Reconnected! IP: %s  Server: %s\n",
                      WiFi.localIP().toString().c_str(), _serverUrl.c_str());
        return true;
    }

private:
    String   _ssid, _password, _serverIp, _serverUrl, _apiKey;
    uint16_t _port = SERVER_PORT;
};
