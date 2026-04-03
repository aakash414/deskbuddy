#pragma once

#include "sprite_coding.h"
#include "sprite_meeting.h"
#include "sprite_rooftop.h"
#include "sprite_focus.h"
#include "sprite_thirsty.h"
#include "sprite_sleeping.h"
#include "sprite_loved.h"

struct SpriteSet {
    const char* name;
    const uint16_t* const* frames;
    int frameCount;
};

static const SpriteSet ALL_SPRITES[] = {
    {"coding", sprite_coding, SPRITE_CODING_FRAMES},
    {"meeting", sprite_meeting, SPRITE_MEETING_FRAMES},
    {"rooftop", sprite_rooftop, SPRITE_ROOFTOP_FRAMES},
    {"focus", sprite_focus, SPRITE_FOCUS_FRAMES},
    {"thirsty", sprite_thirsty, SPRITE_THIRSTY_FRAMES},
    {"sleeping", sprite_sleeping, SPRITE_SLEEPING_FRAMES},
    {"loved", sprite_loved, SPRITE_LOVED_FRAMES},
};
static const int SPRITE_COUNT = 7;
