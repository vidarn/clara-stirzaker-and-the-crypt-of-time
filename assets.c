#include "assets.h"
#include "sprite.h"

Tile tiles[] = {
    {SPRITE_tile},
    {SPRITE_tile_A},
    {SPRITE_tile_B},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
    {SPRITE_tile},
};

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

#define SPAWNER(sprite,name) \
    void spawn_##name(u32 x, u32 y, GameData *gd); 
SPAWNER_LIST
#undef SPAWNER

#define SPAWNER(sprite,name) {sprite, spawn_##name},
Spawner spawners[] = {
SPAWNER_LIST
};
#undef SPAWNER

const u32 num_tiles = sizeof(tiles)/sizeof(Tile);
const u32 num_spawners = sizeof(spawners)/sizeof(Spawner);
