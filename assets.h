#pragma once
#include "main.h"
#include "game.h"
typedef struct
{
    u32 sprite;
}Tile;

typedef struct
{
    void (*spawn_func)(u32 x, u32 y, GameData *gd);
    u32 sprite;
    u32 _pad;
}Spawner;

extern Tile tiles[];
extern const u32 num_tiles;

extern Spawner spawners[];
extern const u32 num_spawners;

#define SPAWNER_LIST \
    SPAWNER(SPRITE_hero,hero) \
    SPAWNER(SPRITE_wall,wall) \
    SPAWNER(SPRITE_wall_switch_A,wall_switch_A) \
    SPAWNER(SPRITE_wall_switch_A_2,wall_switch_A_2) \
    SPAWNER(SPRITE_wall_switch_B,wall_switch_B) \
    SPAWNER(SPRITE_wall_switch_B_2,wall_switch_B_2) \
    SPAWNER(SPRITE_skeleton,skeleton) \
    SPAWNER(SPRITE_gate,gate) \
    SPAWNER(SPRITE_ankh,ankh) \
    SPAWNER(SPRITE_hourglass,hourglass) \
    SPAWNER(SPRITE_crystal,crystal) \

#define SPAWNER(sprite,name) \
    void spawn_##name(u32 x, u32 y, GameData *gd); 
SPAWNER_LIST
#undef SPAWNER

