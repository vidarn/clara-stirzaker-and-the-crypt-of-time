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

#define SPAWNER(sprite,name) {spawn_##name, sprite},
Spawner spawners[] = {
SPAWNER_LIST
};
#undef SPAWNER

const u32 num_tiles = sizeof(tiles)/sizeof(Tile);
const u32 num_spawners = sizeof(spawners)/sizeof(Spawner);
