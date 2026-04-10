#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <LittleFS.h>
#include "config.h"
#include "shared_state.h"

// State-to-animation mapping
struct StateStyle {
    uint16_t   animSpeed;   // bob period ms
};

static const struct {
    const char* name;
    StateStyle  style;
} STATE_DEFS[] = {
    {"happy",       {300}},
    {"coding",      {250}},
    {"meeting",     {300}},
    {"rooftop",     {400}},
    {"focus",       {200}},
    {"idle",        {400}},
    {"thirsty",     {150}},
    {"overwatered", {300}},
    {"loved",       {250}},
    {"sleeping",    {500}},
};
static const int STATE_COUNT = sizeof(STATE_DEFS) / sizeof(STATE_DEFS[0]);

#define SPRITE_W 384
#define SPRITE_H 384
#define FRAME_COUNT 4
#define FRAME_BYTES (SPRITE_W * SPRITE_H * 2)

class Display {
public:
    void begin() {
        buildUI();
        Serial.println("Display UI built");
    }

    void update(const StatusData& data) {
        if (!data.valid) return;

        if (data.state != currentState) {
            currentState = data.state;
            applyState(data.state);
        }

        altTextCount = data.altTextCount;
        for (int i = 0; i < data.altTextCount && i < 2; i++) {
            altTexts[i] = data.alternatingText[i];
        }
        if (altTextCount > 0 && labelBottom)
            lv_label_set_text(labelBottom, altTexts[0].c_str());

    }

    void showOffline() {
        if (currentState == "offline") return;
        currentState = "offline";
        loadFrames("sleeping");
        altTexts[0] = "Offline";
        altTextCount = 1;
        if (labelBottom) lv_label_set_text(labelBottom, altTexts[0].c_str());
    }

    // WiFi connected but server not yet reached — show IP so user can open web UI
    void showConnecting(const String& ip) {
        if (currentState == "connecting") return;
        currentState = "connecting";
        loadFrames("sleeping");   // idle frames may not be uploaded; sleeping always is
        altTexts[0] = ip;
        altTexts[1] = "mbk.local";
        altTextCount = 2;
        if (labelBottom) lv_label_set_text(labelBottom, altTexts[0].c_str());
    }

    // Touch: cycle through demo states
    void nextState() {
        static const char* cycle[] = {"rooftop", "loved", "meeting", "focus", "sleeping"};
        static int idx = 0;
        lastTouchMs = millis();
        currentState = cycle[idx];
        idx = (idx + 1) % 5;
        applyState(currentState);
        altTexts[0] = currentState;
        altTextCount = 1;
        if (labelBottom) lv_label_set_text(labelBottom, currentState.c_str());
    }

    bool inDemoMode() { return lastTouchMs > 0 && millis() - lastTouchMs < DEMO_TOUCH_DURATION_MS; }

    void tick() {
        // Alternate bottom text every TEXT_SWAP_MS
        if (altTextCount > 1 && millis() - lastTextSwap > TEXT_SWAP_MS) {
            currentTextIdx = (currentTextIdx + 1) % altTextCount;
            if (labelBottom)
                lv_label_set_text(labelBottom, altTexts[currentTextIdx].c_str());
            lastTextSwap = millis();
        }

        // Advance animation frame every FRAME_TICK_MS
        if (framesLoaded && millis() - lastFrameTick > FRAME_TICK_MS) {
            currentFrame = (currentFrame + 1) % FRAME_COUNT;
            if (ghostImg && frameDescs[currentFrame].data)
                lv_img_set_src(ghostImg, &frameDescs[currentFrame]);
            lastFrameTick = millis();
        }

    }

private:
    // UI widgets
    lv_obj_t* ghostImg    = nullptr;
    lv_obj_t* labelBottom = nullptr;

    // State
    String currentState   = "sleeping";
    String altTexts[2]    = {"", ""};
    int    altTextCount   = 0;
    int    currentTextIdx = 0;

    // Sprite frame data (PSRAM)
    lv_img_dsc_t frameDescs[FRAME_COUNT] = {};
    uint8_t*     frameBufs[FRAME_COUNT]  = {nullptr, nullptr, nullptr, nullptr};
    bool         framesLoaded            = false;
    int          currentFrame            = 0;

    // Timing
    unsigned long lastTextSwap  = 0;
    unsigned long lastFrameTick = 0;
    unsigned long lastTouchMs   = 0;

    uint16_t   currentAnimSpeed = 300;

    // ── sprite loading ───────────────────────────────────────────────────────

    // Allocate all 4 PSRAM buffers once at startup — reused forever, no fragmentation.
    bool allocFrameBuffers() {
        for (int i = 0; i < FRAME_COUNT; i++) {
            if (frameBufs[i]) continue; // already allocated
            frameBufs[i] = (uint8_t*)heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
            if (!frameBufs[i]) {
                Serial.printf("[display] PSRAM alloc failed for frame buf %d (%u KB free)\n",
                              i, heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
                return false;
            }
            frameDescs[i].header.cf  = LV_IMG_CF_TRUE_COLOR;
            frameDescs[i].header.w   = SPRITE_W;
            frameDescs[i].header.h   = SPRITE_H;
            frameDescs[i].data_size  = FRAME_BYTES;
            frameDescs[i].data       = frameBufs[i];
        }
        Serial.printf("[display] Frame buffers ready (%u KB PSRAM free)\n",
                      heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
        return true;
    }

    void loadFrames(const char* state) {
        framesLoaded = false;

        // Null out data pointers so only successfully-loaded frames are considered valid
        for (int fi = 0; fi < FRAME_COUNT; fi++)
            frameDescs[fi].data = nullptr;

        for (int fi = 0; fi < FRAME_COUNT; fi++) {
            if (!frameBufs[fi]) continue; // buffer not available

            char path[64];
            snprintf(path, sizeof(path), "/sprites/%s_%d.bin", state, fi);

            File f = LittleFS.open(path, "r");
            if (!f) {
                Serial.printf("[display] Missing: %s\n", path);
                continue;
            }
            size_t sz = f.size();
            if (sz != FRAME_BYTES) {
                Serial.printf("[display] Bad size %s: %u (expected %u)\n", path, sz, FRAME_BYTES);
                f.close();
                continue;
            }

            f.read(frameBufs[fi], FRAME_BYTES);
            f.close();
            frameDescs[fi].data = frameBufs[fi]; // mark as valid only after successful read
        }

        // Show first valid frame immediately
        for (int fi = 0; fi < FRAME_COUNT; fi++) {
            if (frameDescs[fi].data) {
                currentFrame = fi;
                if (ghostImg) lv_img_set_src(ghostImg, &frameDescs[fi]);
                framesLoaded = true;
                break;
            }
        }

        // If no frames loaded (sprite not available), fall back to sleeping
        if (!framesLoaded && strcmp(state, "sleeping") != 0) {
            for (int fi = 0; fi < FRAME_COUNT; fi++) {
                if (!frameBufs[fi]) continue;
                char path[64];
                snprintf(path, sizeof(path), "/sprites/sleeping_%d.bin", fi);
                File f = LittleFS.open(path, "r");
                if (!f) continue;
                if (f.size() == FRAME_BYTES) {
                    f.read(frameBufs[fi], FRAME_BYTES);
                    frameDescs[fi].data = frameBufs[fi];
                }
                f.close();
            }
            for (int fi = 0; fi < FRAME_COUNT; fi++) {
                if (frameDescs[fi].data) {
                    currentFrame = fi;
                    if (ghostImg) lv_img_set_src(ghostImg, &frameDescs[fi]);
                    framesLoaded = true;
                    break;
                }
            }
        }

        lastFrameTick = millis();
    }

    // ── state apply ─────────────────────────────────────────────────────────

    void applyState(const String& state) {
        loadFrames(state.c_str());
        startBobAnimation(animSpeedForState(state));
    }

    uint16_t animSpeedForState(const String& state) {
        for (int i = 0; i < STATE_COUNT; i++) {
            if (state == STATE_DEFS[i].name)
                return STATE_DEFS[i].style.animSpeed;
        }
        return 300;
    }

    // ── UI build ─────────────────────────────────────────────────────────────

    void buildUI() {
        lv_obj_t* scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

        ghostImg = lv_img_create(scr);
        lv_obj_set_style_bg_opa(ghostImg, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ghostImg, 0, 0);
        lv_obj_align(ghostImg, LV_ALIGN_CENTER, 0, -24);

        labelBottom = lv_label_create(scr);
        lv_label_set_text(labelBottom, "");
        lv_obj_set_style_text_color(labelBottom, lv_color_hex(0x8A9A8E), 0);
        lv_obj_set_style_text_font(labelBottom, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(labelBottom, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(labelBottom, LCD_WIDTH - 40);
        lv_obj_align(labelBottom, LV_ALIGN_BOTTOM_MID, 0, -24);

        startBobAnimation(300);
        allocFrameBuffers();
        loadFrames("sleeping");
    }

    static void bobAnimCb(void* obj, int32_t v) {
        lv_obj_set_y((lv_obj_t*)obj, v);
    }

    void startBobAnimation(uint16_t periodMs) {
        if (!ghostImg) return;
        currentAnimSpeed = periodMs;
        const int32_t baseY = (LCD_HEIGHT / 2) - 24 - (SPRITE_H / 2);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, ghostImg);
        lv_anim_set_exec_cb(&a, bobAnimCb);
        lv_anim_set_values(&a, baseY - 8, baseY + 8);
        lv_anim_set_time(&a, periodMs * 3);
        lv_anim_set_playback_time(&a, periodMs * 3);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }
};
