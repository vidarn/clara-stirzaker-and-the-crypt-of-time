#pragma once
#include "common.h"
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <assert.h>
#include <SDL2/SDL_keyboard.h>
#ifdef __EMSCRIPTEN__
#include <SDL_ttf.h>
#else
#include <SDL2/SDL_ttf.h>
#endif

typedef struct
{
    float x,y;
}vec2;

typedef struct
{
    u32 x,y;
}uvec2;

typedef struct Tweener Tweener;

typedef struct
{
    u32 currentTime;
    void *data;
}GameStateData;

typedef struct
{
    void (*input)(GameStateData *data, SDL_Event event);
    void (*draw)(GameStateData *data);
    void (*update)(GameStateData *data, float dt);
    GameStateData *data;
    Tweener *tweeners_head;
    Tweener *tweeners_tail;
}GameState;

void quit_game(void);
void draw_text(TTF_Font *font,SDL_Color font_color, const char *message,s32 x,
    s32 y, float center_x, float center_y, float scale);
void draw_text_dialog(const char **messages, u32 num_lines, float scale);

extern SDL_Renderer *renderer;
extern TTF_Font *hud_font;
extern TTF_Font *menu_font;
extern u32 window_w;
extern u32 window_h;
extern u32 tile_w;
extern u32 tile_h;

enum {
    TWEEN_LINEARINTERPOLATION,
        TWEEN_QUADRATICEASEIN,  TWEEN_QUADRATICEASEOUT,  TWEEN_QUADRATICEASEINOUT,
            TWEEN_CUBICEASEIN,      TWEEN_CUBICEASEOUT,      TWEEN_CUBICEASEINOUT,
          TWEEN_QUARTICEASEIN,    TWEEN_QUARTICEASEOUT,    TWEEN_QUARTICEASEINOUT,
          TWEEN_QUINTICEASEIN,    TWEEN_QUINTICEASEOUT,    TWEEN_QUINTICEASEINOUT,
             TWEEN_SINEEASEIN,       TWEEN_SINEEASEOUT,       TWEEN_SINEEASEINOUT,
         TWEEN_CIRCULAREASEIN,   TWEEN_CIRCULAREASEOUT,   TWEEN_CIRCULAREASEINOUT,
      TWEEN_EXPONENTIALEASEIN,TWEEN_EXPONENTIALEASEOUT,TWEEN_EXPONENTIALEASEINOUT,
          TWEEN_ELASTICEASEIN,    TWEEN_ELASTICEASEOUT,    TWEEN_ELASTICEASEINOUT,
             TWEEN_BACKEASEIN,       TWEEN_BACKEASEOUT,       TWEEN_BACKEASEINOUT,
           TWEEN_BOUNCEEASEIN,     TWEEN_BOUNCEEASEOUT,     TWEEN_BOUNCEEASEINOUT,
};
void add_tweener(float *subject, float value, u32 duration, u32 type);

void screen2pixels(float x, float y, u32 *x_out, u32 *y_out);
void pixels2screen(u32 x, u32 y, float *x_out, float *y_out);
