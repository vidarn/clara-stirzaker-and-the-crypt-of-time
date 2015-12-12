#pragma once
#include "main.h"
#include "game.h"
typedef struct
{
    u32 sprite;
}Tile;

typedef struct
{
    u32 sprite;
    void (*spawn_func)(u32 x, u32 y, GameData *gd);
}Spawner;

extern Tile tiles[];
extern const u32 num_tiles;

extern Spawner spawners[];
extern const u32 num_spawners;
