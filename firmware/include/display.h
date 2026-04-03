#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "config.h"
#include "network.h"

// State-to-color mapping for edge glow
struct StateStyle {
    lv_color_t bodyColor;
    lv_color_t glowColor;
    uint8_t glowOpacity;     // 0-255
    uint16_t animSpeed;       // ms per frame
};

static const struct {
    const char* name;
    StateStyle style;
} STATE_STYLES[] = {
    {"happy",       {lv_color_hex(0x4ECB71), lv_color_hex(0x4ECB71), 40,  300}},
    {"coding",      {lv_color_hex(0x4ECB71), lv_color_hex(0x5A9AE6), 50,  250}},
    {"meeting",     {lv_color_hex(0x4ECB71), lv_color_hex(0xE6B84E), 50,  300}},
    {"rooftop",     {lv_color_hex(0x4ECB71), lv_color_hex(0xE6B84E), 40,  400}},
    {"focus",       {lv_color_hex(0x4ECB71), lv_color_hex(0xE67E5A), 60,  200}},
    {"idle",        {lv_color_hex(0x4ECB71), lv_color_hex(0x4ECB71), 30,  400}},
    {"thirsty",     {lv_color_hex(0x7A8A7E), lv_color_hex(0xE64E4E), 80,  150}},
    {"overwatered", {lv_color_hex(0x8ABCE6), lv_color_hex(0x8ABCE6), 70,  300}},
    {"loved",       {lv_color_hex(0xC8ECD4), lv_color_hex(0xE6719A), 60,  250}},
    {"sleeping",    {lv_color_hex(0x5A6A62), lv_color_hex(0x5A6A62), 20,  500}},
};

class Display {
public:
    void begin() {
        buildUI();
        Serial.println("Display UI built");
    }

    void update(const StatusData& data) {
        if (!data.valid) return;

        // Update state if changed
        if (data.state != currentState) {
            currentState = data.state;
            applyStateStyle(data.state);
        }

        // Store alternating texts
        altTextCount = data.altTextCount;
        for (int i = 0; i < data.altTextCount && i < 2; i++) {
            altTexts[i] = data.alternatingText[i];
        }

        // Update glow based on plant health
        if (data.moistureStatus == "dry" || data.moistureStatus == "soggy") {
            setGlow(lv_color_hex(0xE64E4E), 80);
        } else if (data.moistureStatus == "good") {
            applyStateStyle(data.state);
        }
    }

    void showOffline() {
        currentState = "sleeping";
        applyStateStyle("sleeping");
        altTexts[0] = "Offline";
        altTextCount = 1;
    }

    void tick() {
        // Alternate bottom text every 5 seconds
        if (altTextCount > 1 && millis() - lastTextSwap > 5000) {
            currentTextIdx = (currentTextIdx + 1) % altTextCount;
            if (labelBottom) {
                lv_label_set_text(labelBottom, altTexts[currentTextIdx].c_str());
            }
            lastTextSwap = millis();
        }

        // Animate edge glow pulse
        if (millis() - lastGlowTick > 50) {
            animateGlow();
            lastGlowTick = millis();
        }
    }

private:
    // UI elements
    lv_obj_t* spriteArea = nullptr;
    lv_obj_t* labelBottom = nullptr;
    lv_obj_t* glowLeft = nullptr;
    lv_obj_t* glowRight = nullptr;
    lv_obj_t* glowTop = nullptr;
    lv_obj_t* glowBottom = nullptr;

    // Ghost sprite objects
    lv_obj_t* ghostBody = nullptr;
    lv_obj_t* eyeLeft = nullptr;
    lv_obj_t* eyeRight = nullptr;
    lv_obj_t* accentShape = nullptr;

    // State
    String currentState = "happy";
    String altTexts[2] = {"", ""};
    int altTextCount = 0;
    int currentTextIdx = 0;
    unsigned long lastTextSwap = 0;
    unsigned long lastGlowTick = 0;

    // Glow animation
    lv_color_t glowColor = lv_color_hex(0x4ECB71);
    uint8_t glowBaseOpacity = 40;
    float glowPhase = 0;

    // Ghost bob animation state
    uint16_t currentAnimSpeed = 300;

    void buildUI() {
        lv_obj_t* scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

        // Edge glow bars (subtle colored strips on edges)
        glowLeft = createGlowBar(scr, 0, 0, 4, LCD_HEIGHT);
        glowRight = createGlowBar(scr, LCD_WIDTH - 4, 0, 4, LCD_HEIGHT);
        glowTop = createGlowBar(scr, 0, 0, LCD_WIDTH, 4);
        glowBottom = createGlowBar(scr, 0, LCD_HEIGHT - 4, LCD_WIDTH, 4);

        // Sprite container (centered, takes most of screen)
        spriteArea = lv_obj_create(scr);
        lv_obj_set_size(spriteArea, SPRITE_SIZE * SPRITE_SCALE, SPRITE_SIZE * SPRITE_SCALE);
        lv_obj_align(spriteArea, LV_ALIGN_CENTER, 0, -30);
        lv_obj_set_style_bg_opa(spriteArea, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(spriteArea, 0, 0);
        lv_obj_set_style_pad_all(spriteArea, 0, 0);
        lv_obj_set_style_clip_corner(spriteArea, false, 0);

        buildGhost(spriteArea);

        // Bottom text label (alternates every 5s)
        labelBottom = lv_label_create(scr);
        lv_label_set_text(labelBottom, "");
        lv_obj_set_style_text_color(labelBottom, lv_color_hex(0x8A9A8E), 0);
        lv_obj_set_style_text_font(labelBottom, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(labelBottom, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(labelBottom, LCD_WIDTH - 40);
        lv_obj_align(labelBottom, LV_ALIGN_BOTTOM_MID, 0, -24);
    }

    void buildGhost(lv_obj_t* parent) {
        // Ghost body: 180×220, top-rounded pill, centered in spriteArea
        ghostBody = lv_obj_create(parent);
        lv_obj_set_size(ghostBody, 180, 220);
        lv_obj_align(ghostBody, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(ghostBody, lv_color_hex(0x4ECB71), 0);
        lv_obj_set_style_radius(ghostBody, 90, 0);
        lv_obj_set_style_border_width(ghostBody, 0, 0);
        lv_obj_set_style_pad_all(ghostBody, 0, 0);
        lv_obj_clear_flag(ghostBody, LV_OBJ_FLAG_SCROLLABLE);

        // Eyes: white circles, positioned in upper half of body
        eyeLeft = lv_obj_create(ghostBody);
        lv_obj_set_size(eyeLeft, 28, 28);
        lv_obj_set_pos(eyeLeft, 35, 60);
        lv_obj_set_style_bg_color(eyeLeft, lv_color_white(), 0);
        lv_obj_set_style_radius(eyeLeft, 14, 0);
        lv_obj_set_style_border_width(eyeLeft, 0, 0);
        lv_obj_clear_flag(eyeLeft, LV_OBJ_FLAG_SCROLLABLE);

        eyeRight = lv_obj_create(ghostBody);
        lv_obj_set_size(eyeRight, 28, 28);
        lv_obj_set_pos(eyeRight, 117, 60);
        lv_obj_set_style_bg_color(eyeRight, lv_color_white(), 0);
        lv_obj_set_style_radius(eyeRight, 14, 0);
        lv_obj_set_style_border_width(eyeRight, 0, 0);
        lv_obj_clear_flag(eyeRight, LV_OBJ_FLAG_SCROLLABLE);

        // Accent shape: small 36×36 circle at bottom-center, hidden by default
        accentShape = lv_obj_create(ghostBody);
        lv_obj_set_size(accentShape, 36, 36);
        lv_obj_align(accentShape, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_bg_color(accentShape, lv_color_hex(0xE6719A), 0);
        lv_obj_set_style_radius(accentShape, 18, 0);
        lv_obj_set_style_border_width(accentShape, 0, 0);
        lv_obj_add_flag(accentShape, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(accentShape, LV_OBJ_FLAG_SCROLLABLE);

        // Start bob animation
        startBobAnimation(300);
    }

    static void bobAnimCallback(void* obj, int32_t v) {
        lv_obj_set_y((lv_obj_t*)obj, v);
    }

    void startBobAnimation(uint16_t periodMs) {
        if (!ghostBody) return;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, ghostBody);
        lv_anim_set_exec_cb(&a, bobAnimCallback);
        // Center Y for ghostBody inside 336px container: (336-220)/2 = 58
        lv_anim_set_values(&a, 50, 66);  // bob 8px above/below center
        lv_anim_set_time(&a, periodMs * 4);
        lv_anim_set_playback_time(&a, periodMs * 4);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
        currentAnimSpeed = periodMs;
    }

    lv_obj_t* createGlowBar(lv_obj_t* parent, int x, int y, int w, int h) {
        lv_obj_t* bar = lv_obj_create(parent);
        lv_obj_set_pos(bar, x, y);
        lv_obj_set_size(bar, w, h);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x4ECB71), 0);
        lv_obj_set_style_bg_opa(bar, 40, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        return bar;
    }

    void setGlow(lv_color_t color, uint8_t opacity) {
        glowColor = color;
        glowBaseOpacity = opacity;
        lv_obj_set_style_bg_color(glowLeft, color, 0);
        lv_obj_set_style_bg_color(glowRight, color, 0);
        lv_obj_set_style_bg_color(glowTop, color, 0);
        lv_obj_set_style_bg_color(glowBottom, color, 0);
    }

    void animateGlow() {
        glowPhase += 0.05f;
        if (glowPhase > 6.28f) glowPhase -= 6.28f;

        // Sine wave pulse: opacity oscillates around base
        float factor = 0.5f + 0.5f * sin(glowPhase);
        uint8_t opa = (uint8_t)(glowBaseOpacity * (0.6f + 0.4f * factor));

        lv_obj_set_style_bg_opa(glowLeft, opa, 0);
        lv_obj_set_style_bg_opa(glowRight, opa, 0);
        lv_obj_set_style_bg_opa(glowTop, opa, 0);
        lv_obj_set_style_bg_opa(glowBottom, opa, 0);
    }

    void applyStateStyle(const String& state) {
        for (const auto& s : STATE_STYLES) {
            if (state == s.name) {
                setGlow(s.style.glowColor, s.style.glowOpacity);
                applyGhostStyle(state, s.style);
                return;
            }
        }
        setGlow(lv_color_hex(0x4ECB71), 40);
        applyGhostStyle("happy", STATE_STYLES[0].style);
    }

    void applyGhostStyle(const String& state, const StateStyle& style) {
        if (!ghostBody) return;

        // Body color and opacity
        lv_obj_set_style_bg_color(ghostBody, style.bodyColor, 0);
        lv_obj_set_style_bg_opa(ghostBody, LV_OPA_COVER, 0);

        // Eye visibility
        bool showEyes = (state != "sleeping");
        if (eyeLeft)  { showEyes ? lv_obj_clear_flag(eyeLeft, LV_OBJ_FLAG_HIDDEN)  : lv_obj_add_flag(eyeLeft, LV_OBJ_FLAG_HIDDEN); }
        if (eyeRight) { showEyes ? lv_obj_clear_flag(eyeRight, LV_OBJ_FLAG_HIDDEN) : lv_obj_add_flag(eyeRight, LV_OBJ_FLAG_HIDDEN); }

        // Accent shape per state
        if (accentShape) {
            lv_obj_add_flag(accentShape, LV_OBJ_FLAG_HIDDEN);  // hide by default

            if (state == "thirsty") {
                lv_obj_set_style_bg_color(accentShape, lv_color_hex(0xE64E4E), 0);
                lv_obj_clear_flag(accentShape, LV_OBJ_FLAG_HIDDEN);
            } else if (state == "loved") {
                lv_obj_set_style_bg_color(accentShape, lv_color_hex(0xE6719A), 0);
                lv_obj_clear_flag(accentShape, LV_OBJ_FLAG_HIDDEN);
            } else if (state == "overwatered") {
                lv_obj_set_style_bg_color(accentShape, lv_color_hex(0x8ABCE6), 0);
                lv_obj_clear_flag(accentShape, LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Sleeping: dim the whole ghost
        if (state == "sleeping") {
            lv_obj_set_style_bg_opa(ghostBody, 120, 0);
        }

        // Restart bob at new speed if changed
        if (style.animSpeed != currentAnimSpeed) {
            startBobAnimation(style.animSpeed);
        }
    }
};
