#pragma once
#include "main.h"

typedef struct{
    s32 w,h;
    s32 layers;
    s16 *tiles;
}Map;
Map load_map(const char *filename);
void save_map(const char *filename, Map map);
void delete_map(Map map);
