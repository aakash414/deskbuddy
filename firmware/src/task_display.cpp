#include <Arduino.h>
#include <lvgl.h>
#include "shared_state.h"
#include "display.h"

extern Display display;
extern volatile bool touchFlag;

void taskDisplay(void* param) {
    bool lastTouchFlag = false;
    for (;;) {
        // Sample touch BEFORE lv_timer_handler — lvgl_touch_cb clears touchFlag inside it
        bool t = touchFlag;
        if (t && !lastTouchFlag) display.nextState();
        lastTouchFlag = t;

        lv_timer_handler();

        // Snapshot shared state — short critical section, no LVGL calls inside
        lockState();
        bool apMode   = gApMode;
        bool wifiOk   = gWifiConnected;
        String devIP  = gDeviceIP;
        StatusData st = gStatus;
        unlockState();

        if (apMode) {
            display.showSetupMode(devIP);
        } else if (display.inDemoMode()) {
            // touch cycling active — don't override with offline/status
        } else if (!wifiOk) {
            display.showOffline();
        } else if (!st.valid) {
            // WiFi up but server not yet reached — show IP so user can find web UI
            display.showConnecting(devIP);
        } else {
            display.update(st);
        }

        display.tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
