#pragma once
#include "main.h"

#define SOUND_LIST \
SOUND(slide) \
SOUND(chime) \
SOUND(step) \
SOUND(bell) \
SOUND(hurt1) \
SOUND(hurt2) \
SOUND(hurt3) \
SOUND(hurt4) \

enum
{
#define SOUND(name) SOUND_##name,
    SOUND_LIST
#undef SOUND
    NUM_SOUNDS
};

void sound_init(void);
void sound_quit(void);
void play_sound(u32 sound, float volume);
void play_music(void);
