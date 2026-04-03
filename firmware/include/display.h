#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <LittleFS.h>
#include "config.h"
#include "shared_state.h"

// State-to-glow mapping
struct StateStyle {
    lv_color_t glowColor;
    uint8_t    glowOpacity;
    uint16_t   animSpeed;   // bob period ms
};

static const struct {
    const char* name;
    StateStyle  style;
} STATE_DEFS[] = {
    {"happy",       {lv_color_hex(0x4ECB71), 40, 300}},
    {"coding",      {lv_color_hex(0x5A9AE6), 50, 250}},
    {"meeting",     {lv_color_hex(0xE6B84E), 50, 300}},
    {"rooftop",     {lv_color_hex(0xE6B84E), 40, 400}},
    {"focus",       {lv_color_hex(0xE67E5A), 60, 200}},
    {"idle",        {lv_color_hex(0x4ECB71), 30, 400}},
    {"thirsty",     {lv_color_hex(0xE64E4E), 80, 150}},
    {"overwatered", {lv_color_hex(0x8ABCE6), 70, 300}},
    {"loved",       {lv_color_hex(0xE6719A), 60, 250}},
    {"sleeping",    {lv_color_hex(0x5A6A62), 20, 500}},
};
static const int STATE_COUNT = sizeof(STATE_DEFS) / sizeof(STATE_DEFS[0]);

#define SPRITE_W 360
#define SPRITE_H 360
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

        if (data.moistureStatus == "dry" || data.moistureStatus == "soggy") {
            setGlow(lv_color_hex(0xE64E4E), 80);
        }
    }

    void showOffline() {
        if (currentState == "offline") return;
        currentState = "offline";
        loadFrames("sleeping");
        applyGlowForState("sleeping");
        altTexts[0] = "Offline";
        altTextCount = 1;
        if (labelBottom) lv_label_set_text(labelBottom, altTexts[0].c_str());
    }

    // WiFi connected but server not yet reached — show IP so user can open web UI
    void showConnecting(const String& ip) {
        if (currentState == "connecting") return;
        currentState = "connecting";
        loadFrames("sleeping");   // idle frames may not be uploaded; sleeping always is
        applyGlowForState("idle");
        altTexts[0] = ip;
        altTexts[1] = "deskbuddy.local";
        altTextCount = 2;
        if (labelBottom) lv_label_set_text(labelBottom, altTexts[0].c_str());
    }

    void showSetupMode(const String& ip = "192.168.4.1") {
        if (currentState == "setup") return;
        currentState = "setup";
        loadFrames("sleeping");
        applyGlowForState("sleeping");
        altTexts[0] = "Setup mode";
        altTexts[1] = ip;
        altTextCount = 2;
        if (labelBottom) lv_label_set_text(labelBottom, altTexts[0].c_str());
    }

    // Touch: cycle rooftop → loved → sleeping
    void nextState() {
        static const char* cycle[] = {"rooftop", "loved", "sleeping"};
        static int idx = 0;
        lastTouchMs = millis();
        currentState = cycle[idx];
        idx = (idx + 1) % 3;
        applyState(currentState);
        altTexts[0] = currentState;
        altTextCount = 1;
        if (labelBottom) lv_label_set_text(labelBottom, currentState.c_str());
    }

    bool inDemoMode() { return millis() - lastTouchMs < 30000; }

    void tick() {
        // Alternate bottom text every 5 s
        if (altTextCount > 1 && millis() - lastTextSwap > 5000) {
            currentTextIdx = (currentTextIdx + 1) % altTextCount;
            if (labelBottom)
                lv_label_set_text(labelBottom, altTexts[currentTextIdx].c_str());
            lastTextSwap = millis();
        }

        // Advance animation frame every 200 ms
        if (framesLoaded && millis() - lastFrameTick > 200) {
            currentFrame = (currentFrame + 1) % FRAME_COUNT;
            if (ghostImg && frameDescs[currentFrame].data)
                lv_img_set_src(ghostImg, &frameDescs[currentFrame]);
            lastFrameTick = millis();
        }

        // Pulse edge glow
        if (millis() - lastGlowTick > 50) {
            animateGlow();
            lastGlowTick = millis();
        }
    }

private:
    // UI widgets
    lv_obj_t* ghostImg    = nullptr;
    lv_obj_t* labelBottom = nullptr;
    lv_obj_t* glowLeft    = nullptr;
    lv_obj_t* glowRight   = nullptr;
    lv_obj_t* glowTop     = nullptr;
    lv_obj_t* glowBottom  = nullptr;

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
    unsigned long lastGlowTick  = 0;
    unsigned long lastFrameTick = 0;
    unsigned long lastTouchMs   = 0;

    // Glow
    lv_color_t glowColor       = lv_color_hex(0x5A6A62);
    uint8_t    glowBaseOpacity = 20;
    float      glowPhase       = 0;
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
        lastFrameTick = millis();
    }

    // ── state apply ─────────────────────────────────────────────────────────

    void applyState(const String& state) {
        loadFrames(state.c_str());
        applyGlowForState(state);
        startBobAnimation(animSpeedForState(state));
    }

    void applyGlowForState(const String& state) {
        for (int i = 0; i < STATE_COUNT; i++) {
            if (state == STATE_DEFS[i].name) {
                setGlow(STATE_DEFS[i].style.glowColor, STATE_DEFS[i].style.glowOpacity);
                return;
            }
        }
        setGlow(lv_color_hex(0x4ECB71), 40);
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

        glowLeft   = createGlowBar(scr, 0,             0,              4,         LCD_HEIGHT);
        glowRight  = createGlowBar(scr, LCD_WIDTH - 4, 0,              4,         LCD_HEIGHT);
        glowTop    = createGlowBar(scr, 0,             0,              LCD_WIDTH, 4);
        glowBottom = createGlowBar(scr, 0,             LCD_HEIGHT - 4, LCD_WIDTH, 4);

        ghostImg = lv_img_create(scr);
        lv_obj_set_style_bg_opa(ghostImg, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ghostImg, 0, 0);
        lv_obj_align(ghostImg, LV_ALIGN_CENTER, 0, -20);

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

    lv_obj_t* createGlowBar(lv_obj_t* parent, int x, int y, int w, int h) {
        lv_obj_t* bar = lv_obj_create(parent);
        lv_obj_set_pos(bar, x, y);
        lv_obj_set_size(bar, w, h);
        lv_obj_set_style_bg_color(bar, glowColor, 0);
        lv_obj_set_style_bg_opa(bar, glowBaseOpacity, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        return bar;
    }

    void setGlow(lv_color_t color, uint8_t opacity) {
        glowColor = color;
        glowBaseOpacity = opacity;
        for (auto* bar : {glowLeft, glowRight, glowTop, glowBottom})
            lv_obj_set_style_bg_color(bar, color, 0);
    }

    void animateGlow() {
        glowPhase += 0.05f;
        if (glowPhase > 6.28f) glowPhase -= 6.28f;
        float   factor = 0.5f + 0.5f * sin(glowPhase);
        uint8_t opa    = (uint8_t)(glowBaseOpacity * (0.6f + 0.4f * factor));
        for (auto* bar : {glowLeft, glowRight, glowTop, glowBottom})
            lv_obj_set_style_bg_opa(bar, opa, 0);
    }

    static void bobAnimCb(void* obj, int32_t v) {
        lv_obj_set_y((lv_obj_t*)obj, v);
    }

    void startBobAnimation(uint16_t periodMs) {
        if (!ghostImg) return;
        currentAnimSpeed = periodMs;
        const int32_t baseY = (LCD_HEIGHT / 2) - 20 - (SPRITE_H / 2);
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
