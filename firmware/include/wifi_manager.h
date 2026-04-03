#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "config.h"
#include "shared_state.h"

// Forward declaration — defined in task_config_ui.cpp
void taskConfigUI(void* param);

class WiFiManager {
public:
    void begin() {
        // Load credentials — use isKey() to avoid noisy "NOT_FOUND" log spam on first boot
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
            esp_wifi_set_ps(WIFI_PS_NONE);  // max throughput, no idle throttling
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
            Serial.printf("[WiFi] Failed (status=%d), starting AP\n", (int)WiFi.status());
            WiFi.mode(WIFI_AP);
            WiFi.softAP(WIFI_AP_SSID);
            vTaskDelay(pdMS_TO_TICKS(100));
            lockState();
            gApMode = true;
            gDeviceIP = WiFi.softAPIP().toString();
            unlockState();
            Serial.printf("[AP] SSID: %s  IP: %s\n",
                          WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
        }

        // Web config server runs in both STA and AP modes
        startWebServer();
    }

    // Called from taskConfigUI every loop iteration
    void handleClient() {
        _server.handleClient();
        if (_pendingRestart) {
            vTaskDelay(pdMS_TO_TICKS(1500));
            ESP.restart();
        }
    }

    bool   isConnected()  { return gWifiConnected; }
    bool   isApMode()     { return gApMode; }
    String getServerUrl() { return _serverUrl; }
    String getApiKey()    { return _apiKey; }

private:
    WebServer _server{80};
    bool      _pendingRestart = false;

    String   _ssid, _password, _serverIp, _serverUrl, _apiKey;
    uint16_t _port = SERVER_PORT;

    void startWebServer() {
        _server.on("/",       HTTP_GET,  [this](){ serveRoot(); });
        _server.on("/save",   HTTP_POST, [this](){ handleSave(); });
        _server.on("/scan",   HTTP_GET,  [this](){ handleScan(); });
        _server.on("/status", HTTP_GET,  [this](){ handleStatus(); });
        _server.begin();

        xTaskCreatePinnedToCore(taskConfigUI, "configui",
                                STACK_CONFIG_UI, nullptr, 1,
                                &gConfigUIHandle, 0);

        if (gApMode) {
            Serial.printf("[web] Config UI at http://%s\n", WiFi.softAPIP().toString().c_str());
        } else {
            Serial.printf("[web] Config UI at http://%s  (or http://deskbuddy.local)\n",
                          WiFi.localIP().toString().c_str());
        }
    }

    // Escape string for safe HTML attribute insertion
    static String htmlEscape(const String& s) {
        String out;
        out.reserve(s.length() + 8);
        for (size_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if      (c == '&')  out += "&amp;";
            else if (c == '"')  out += "&quot;";
            else if (c == '<')  out += "&lt;";
            else if (c == '>')  out += "&gt;";
            else                out += c;
        }
        return out;
    }

    void serveRoot() {
        String page = configPageHtml();

        // Status badge — different in AP vs STA mode
        String badge;
        if (gApMode) {
            badge = "&#x26A1; AP Mode &mdash; connect to <b>DeskBuddy-Setup</b>, then open this page";
        } else {
            badge = "&#x2705; Connected to <b>" + htmlEscape(_ssid) + "</b>"
                    " &mdash; " + WiFi.localIP().toString();
        }
        page.replace("__STATUS_HTML__", badge);
        page.replace("__SSID__",        htmlEscape(gApMode ? "" : _ssid));
        page.replace("__SERVER_IP__",   htmlEscape(_serverIp));
        page.replace("__SERVER_PORT__", String(_port));
        page.replace("__API_KEY__",     htmlEscape(_apiKey));
        _server.send(200, "text/html", page);
    }

    void handleScan() {
        int n = WiFi.scanNetworks();
        String json;
        json.reserve(n * 64);
        json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            String ssid = WiFi.SSID(i);
            ssid.replace("\\", "\\\\");
            ssid.replace("\"", "\\\"");
            json += "{\"ssid\":\""   + ssid + "\""
                  + ",\"rssi\":"     + String(WiFi.RSSI(i))
                  + ",\"encrypted\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false")
                  + "}";
        }
        json += "]";
        _server.send(200, "application/json", json);
    }

    void handleStatus() {
        String ip   = gApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        String mode = gApMode ? "ap" : "sta";
        String ssid = _ssid;
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        String json = "{\"mode\":\"" + mode + "\""
                      ",\"ssid\":\""  + ssid + "\""
                      ",\"ip\":\""    + ip   + "\"}";
        _server.send(200, "application/json", json);
    }

    void handleSave() {
        String newSsid    = _server.arg("ssid");
        String newPass    = _server.arg("password");
        String newIp      = _server.arg("server_ip");
        String portStr    = _server.arg("server_port");
        String newApiKey  = _server.arg("api_key");

        if (newSsid.isEmpty()) {
            _server.send(400, "text/plain", "SSID required");
            return;
        }

        Preferences prefs;
        prefs.begin("deskbuddy", false);
        prefs.putString("ssid",     newSsid);
        prefs.putString("password", newPass);
        if (!newIp.isEmpty())    prefs.putString("server_ip",  newIp);
        if (!portStr.isEmpty())  prefs.putUInt("server_port",  (uint16_t)portStr.toInt());
        if (!newApiKey.isEmpty()) prefs.putString("api_key",   newApiKey);
        prefs.end();

        Serial.printf("[web] Saved SSID='%s', rebooting...\n", newSsid.c_str());
        _server.send(200, "text/html",
            "<!DOCTYPE html><html><head><meta charset='utf-8'></head>"
            "<body style='background:#111;color:#4ECB71;font-family:sans-serif;"
            "text-align:center;padding:60px 20px'>"
            "<h2>Saved!</h2><p style='color:#aaa'>Rebooting in 2 seconds&hellip;</p>"
            "</body></html>");

        _pendingRestart = true;
    }

    static String configPageHtml() {
        return F(R"rawhtml(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DeskBuddy Config</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;background:#111;color:#eee;max-width:480px;margin:0 auto;padding:20px 16px}
h1{color:#4ECB71;margin:0 0 2px}
.sub{color:#888;font-size:13px;margin:0 0 16px}
.card{background:#1a1a1a;border-radius:10px;padding:16px;margin-bottom:14px}
.card h2{margin:0 0 12px;font-size:14px;color:#4ECB71;text-transform:uppercase;letter-spacing:.05em}
label{display:block;font-size:12px;color:#888;margin-top:10px;margin-bottom:3px}
label:first-of-type{margin-top:0}
input,select{width:100%;background:#252525;border:1px solid #383838;color:#eee;
  padding:9px 10px;border-radius:6px;font-size:14px;outline:none}
input:focus,select:focus{border-color:#4ECB71}
.row{display:flex;gap:8px}
.btn{display:block;width:100%;padding:13px;background:#4ECB71;color:#000;border:none;
  border-radius:8px;font-size:15px;font-weight:700;cursor:pointer;margin-top:4px}
.btn:active{background:#3ab862}
.scan-btn{background:#252525;color:#aaa;border:1px solid #383838;padding:8px 12px;
  border-radius:6px;font-size:12px;cursor:pointer;margin-top:8px;width:auto}
.scan-btn:active{background:#333}
#msg{display:none;background:#1a2e1a;border:1px solid #4ECB71;border-radius:8px;
  padding:12px;margin-top:12px;text-align:center;color:#4ECB71;font-size:14px}
.badge{background:#252525;border-radius:6px;padding:8px 12px;font-size:13px;
  color:#aaa;margin-bottom:14px;line-height:1.5}
</style>
</head>
<body>
<h1>DeskBuddy</h1>
<p class="sub">Device Configuration</p>
<div class="badge">__STATUS_HTML__</div>
<form id="cfg" onsubmit="doSave(event)">
  <div class="card">
    <h2>WiFi</h2>
    <label>Available networks</label>
    <select id="scanSel" onchange="if(this.value)document.getElementById('ssid').value=this.value">
      <option value="">-- tap Scan to load --</option>
    </select>
    <button type="button" class="scan-btn" onclick="doScan()">&#x1F50D; Scan</button>
    <label>SSID</label>
    <input id="ssid" name="ssid" required value="__SSID__" placeholder="Network name" autocomplete="off">
    <label>Password</label>
    <input name="password" type="password" placeholder="Leave empty for open networks" autocomplete="new-password">
  </div>
  <div class="card">
    <h2>Server</h2>
    <label>Server IP</label>
    <input name="server_ip" value="__SERVER_IP__" placeholder="192.168.1.100">
    <div class="row">
      <div style="flex:1">
        <label>Port</label>
        <input name="server_port" type="number" value="__SERVER_PORT__" placeholder="3777">
      </div>
      <div style="flex:2">
        <label>API Key</label>
        <input name="api_key" value="__API_KEY__" placeholder="your_api_key" autocomplete="off">
      </div>
    </div>
  </div>
  <button type="submit" class="btn">Save &amp; Reboot</button>
  <div id="msg">&#x2713; Saved! Device is rebooting&hellip;</div>
</form>
<script>
function doScan(){
  var sel=document.getElementById('scanSel');
  sel.innerHTML='<option>Scanning\u2026</option>';
  fetch('/scan').then(function(r){return r.json()}).then(function(nets){
    var s='<option value="">-- select a network --</option>';
    nets.sort(function(a,b){return b.rssi-a.rssi});
    for(var i=0;i<nets.length;i++){
      var n=nets[i];
      s+='<option value="'+n.ssid.replace(/&/g,'&amp;').replace(/"/g,'&quot;')+'">'+
         n.ssid+(n.encrypted?' &#x1F512;':'')+' ('+n.rssi+' dBm)</option>';
    }
    sel.innerHTML=s;
  }).catch(function(){sel.innerHTML='<option>Scan failed \u2014 try again</option>';});
}
function doSave(e){
  e.preventDefault();
  var d=new URLSearchParams(new FormData(document.getElementById('cfg')));
  fetch('/save',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:d.toString()})
  .then(function(r){
    if(r.ok){document.getElementById('msg').style.display='block';}
    else{alert('Save failed ('+r.status+')');}
  }).catch(function(){alert('Request failed');});
}
</script>
</body>
</html>)rawhtml");
    }
};
