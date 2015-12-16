#pragma once
#include "main.h"

typedef struct{
    s16 *tiles;
    s32 w,h;
    s32 layers;
    s32 _pad;
}Map;
Map load_map(const char *filename);
void save_map(const char *filename, Map map);
void delete_map(Map map);
