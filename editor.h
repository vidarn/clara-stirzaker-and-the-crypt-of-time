#pragma once
#include "main.h"
GameState create_editor_state(u32 window_w, u32 window_h);

typedef struct{
    s32 w,h;
    s32 layers;
    s16 *tiles;
}Map;
Map load_map(const char *filename);
void delete_map(Map map);
