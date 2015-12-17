#include "main.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "editor.h"
#include "game.h"
#include "sprite.h"
#include "sound.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "easing.h"

static int GAME_QUIT = 0;
void quit_game()
{
    GAME_QUIT = 1;
}
SDL_Renderer *renderer;

static GameState editor_state;
static GameState game_state;

static GameState *current_state;

u32 window_w = 1200;
u32 window_h = 800;
u32 tile_w = 168;
u32 tile_h = 118;

struct Tweener
{
    float *subject;
    Tweener *next;
    float  orig;
    float  value;
    u32    duration;
    u32    time;
    u32    type;
    u32 _pad;
};

void add_tweener(float *subject, float value, u32 duration, u32 type)
{
    u8 found_existing = 0;
    Tweener *current = current_state->tweeners_head;
    //NOTE(Vidar): replace existing tweener if it points at the same variable
    while(current != 0) {
        if(current->subject == subject){
            found_existing = 1;
            current->orig     = *subject;
            current->value    = value;
            current->duration = duration;
            current->time     = 0;
            current->type     = type;
            break;
        }
        current = current->next;
    }

    if(!found_existing){
        Tweener *e  = malloc(sizeof(Tweener));
        e->orig     = *subject;
        e->value    = value;
        e->duration = duration;
        e->time     = 0;
        e->type     = type;
        e->subject  = subject;
        e->next     = 0;
        if(current_state->tweeners_tail != 0){
            current_state->tweeners_tail->next = e;
        }
        current_state->tweeners_tail = e;
        if(current_state->tweeners_head == 0){
            current_state->tweeners_head = e;
        }
    }
}


static double apply_tweening(float t, u32 type){
    switch(type){
        case TWEEN_LINEARINTERPOLATION: return LinearInterpolation(t);
        case TWEEN_QUADRATICEASEIN:     return QuadraticEaseIn(t);
        case TWEEN_QUADRATICEASEOUT:    return QuadraticEaseOut(t);
        case TWEEN_QUADRATICEASEINOUT:  return QuadraticEaseInOut(t);
        case TWEEN_CUBICEASEIN:         return CubicEaseIn(t);
        case TWEEN_CUBICEASEOUT:        return CubicEaseOut(t);
        case TWEEN_CUBICEASEINOUT:      return CubicEaseInOut(t);
        case TWEEN_QUARTICEASEIN:       return QuarticEaseIn(t);
        case TWEEN_QUARTICEASEOUT:      return QuarticEaseOut(t);
        case TWEEN_QUARTICEASEINOUT:    return QuarticEaseInOut(t);
        case TWEEN_QUINTICEASEIN:       return QuinticEaseIn(t);
        case TWEEN_QUINTICEASEOUT:      return QuinticEaseOut(t);
        case TWEEN_QUINTICEASEINOUT:    return QuinticEaseInOut(t);
        case TWEEN_SINEEASEIN:          return SineEaseIn(t);
        case TWEEN_SINEEASEOUT:         return SineEaseOut(t);
        case TWEEN_SINEEASEINOUT:       return SineEaseInOut(t);
        case TWEEN_CIRCULAREASEIN:      return CircularEaseIn(t);
        case TWEEN_CIRCULAREASEOUT:     return CircularEaseOut(t);
        case TWEEN_CIRCULAREASEINOUT:   return CircularEaseInOut(t);
        case TWEEN_EXPONENTIALEASEIN:   return ExponentialEaseIn(t);
        case TWEEN_EXPONENTIALEASEOUT:  return ExponentialEaseOut(t);
        case TWEEN_EXPONENTIALEASEINOUT:return ExponentialEaseInOut(t);
        case TWEEN_ELASTICEASEIN:       return ElasticEaseIn(t);
        case TWEEN_ELASTICEASEOUT:      return ElasticEaseOut(t);
        case TWEEN_ELASTICEASEINOUT:    return ElasticEaseInOut(t);
        case TWEEN_BACKEASEIN:          return BackEaseIn(t);
        case TWEEN_BACKEASEOUT:         return BackEaseOut(t);
        case TWEEN_BACKEASEINOUT:       return BackEaseInOut(t);
        case TWEEN_BOUNCEEASEIN:        return BounceEaseIn(t);
        case TWEEN_BOUNCEEASEOUT:       return BounceEaseOut(t);
        case TWEEN_BOUNCEEASEINOUT:     return BounceEaseInOut(t);
        default:                        assert(0); return 0.f;
    }
}

static void updateTweeners(u32 delta_ticks)
{
    Tweener *current = current_state->tweeners_head;
    Tweener *previous = 0;
    while(current != 0) {
        u8 free_current = 0;
        current->time += delta_ticks;
        if(current->time > current->duration){
            if(previous != 0){
                previous->next = current->next;
            }else{
                current_state->tweeners_head = current->next;
            }
            if(current_state->tweeners_tail == current){
                current_state->tweeners_tail = previous;
            }
            current->time = current->duration;
            free_current = 1;
        }
        float t = (float)current->time/(float)current->duration;
        float x = (float)apply_tweening(t,current->type);
        *current->subject = (1.f-x)*current->orig
            + x*(current->value);
        if(free_current){
            Tweener *tmp = current;
            current = current->next;
            free(tmp);
        }else{
            previous = current;
            current = current->next;
        }
    }
}

void screen2pixels(float x, float y, u32 *x_out, u32 *y_out)
{
    *x_out = (u32)(x*window_w);
    *y_out = (u32)(y*window_h);
}

void pixels2screen(u32 x, u32 y, float *x_out, float *y_out)
{
    *x_out = (float)x/(float)window_w;
    *y_out = (float)y/(float)window_h;
}

TTF_Font *hud_font = NULL;
TTF_Font *menu_font = NULL;

void draw_text(TTF_Font *font,SDL_Color font_color, const char *message,s32 x,
        s32 y, float center_x, float center_y, float scale){
    SDL_Surface *surf = TTF_RenderText_Blended(font, message, font_color);
	if (surf != 0){
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
        if (texture != 0){
            int iw, ih;
            SDL_QueryTexture(texture, NULL, NULL, &iw, &ih);
            s32 w = (s32)((float)iw*scale);
            s32 h = (s32)((float)ih*scale);
            SDL_Rect dest_rect = {x-(s32)(center_x*w),y-(s32)(center_y*h),w,h};
            SDL_RenderCopy(renderer,texture,NULL,&dest_rect);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surf);
	}
}

void draw_text_dialog(const char **messages, u32 num_lines, float scale)
{
    float line_h = 60.f;
    float min_w = 300.f;
    float min_h = 150.f;
    SDL_Texture **textures = malloc(sizeof(SDL_Texture*)*num_lines);
    float *widths = malloc(sizeof(float)*num_lines);
    float *heights = malloc(sizeof(float)*num_lines);
    float max_width = 0.f;
    memset(textures,0,num_lines*sizeof(SDL_Texture*));
    memset(widths,  0,num_lines*sizeof(float));
    memset(heights, 0,num_lines*sizeof(float));
    SDL_Color font_color = {62,44,33,255};
    for(u32 i = 0; i < num_lines;i++){
        SDL_Surface *surf = TTF_RenderText_Blended(menu_font, messages[i], font_color);
        if (surf != 0){
            textures[i] = SDL_CreateTextureFromSurface(renderer, surf);
            if (textures[i] != 0){
                int iw, ih;
                SDL_QueryTexture(textures[i], NULL, NULL, &iw, &ih);
                widths[i]  = (float)iw;
                heights[i] = (float)ih;
                if(widths[i] > max_width){
                    max_width = widths[i];
                }
            }
            SDL_FreeSurface(surf);
        }
    }
    float border_x = max_float(80.f,min_w-0.5f*max_width);
    float border_y = max_float(40.f,min_h-0.5f*num_lines*line_h);
    u32 x,y;
    screen2pixels(0.5f,0.5f,&x,&y);
    x-=0.5f*max_width*scale+border_x*scale;
    y-=0.5f*line_h*num_lines*scale+border_y*scale;
    draw_dialog(x,y,max_width+2*border_x,line_h*num_lines+2*border_y,scale);
    for(u32 i = 0;i<num_lines;i++){
        if(textures[i] != 0){
            SDL_Rect dest_rect = {(s32)x+(s32)(border_x*scale),
                (s32)y+(s32)((i*line_h+border_y)*scale),
                (s32)(widths[i]*scale),(s32)(heights[i]*scale)};
            SDL_RenderCopy(renderer,textures[i],NULL,&dest_rect);
            SDL_DestroyTexture(textures[i]);
        }
    }
    free(textures);
    free(widths);
    free(heights);
}


static u32 last_tick = 0;
static float last_dt = 0;
#define num_prev_ticks 16
static u32 current_prev_ticks = 0;
static u32 prev_ticks[num_prev_ticks] = {};

static void draw_fps()
{
#ifdef DEBUG
    float dt = 0.f;
    for(int i=0;i<num_prev_ticks;i++){
        dt += prev_ticks[i]/1000.f;
    }
    dt *= 1.f/(float)num_prev_ticks;
    char fps_buffer[32] = {};
    SDL_Color font_color = {0,0,0,255};
    sprintf(fps_buffer,"fps: %1.2f",1.f/dt);
    u32 x,y;
    screen2pixels(1.f,0.f,&x,&y);
    draw_text(hud_font,font_color,fps_buffer,(s32)x,(s32)y,1.f,0.f,1.f);
#endif
}

static void main_callback(UNUSED void * vdata)
{
    u32 current_tick = SDL_GetTicks();
    u32 delta_ticks = current_tick - last_tick;
    float dt = (float)delta_ticks/1000.f;
    last_tick = current_tick;
    last_dt = dt;
    current_prev_ticks = (current_prev_ticks + 1) % num_prev_ticks;
    prev_ticks[current_prev_ticks] = delta_ticks;

    // Input
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type){
            case SDL_QUIT: {
                quit_game();
            }break;
            case SDL_WINDOWEVENT: {
                switch (event.window.event){
                    case SDL_WINDOWEVENT_RESIZED:
                        window_w = (u32)event.window.data1;
                        window_h = (u32)event.window.data2;
                    break;
                    default:break;
                }
            }break;
            case SDL_KEYDOWN: {
#ifdef DEBUG
                switch(event.key.keysym.sym){
                    case SDLK_F2:
                        current_state = &editor_state;
                        break;
                    case SDLK_F3:
                        current_state = &game_state;
                        break;
                }
#endif
            }break;
        }
        current_state->input(current_state->data,event);
    }
    // Update
    updateTweeners(delta_ticks);
    current_state->update(current_state->data,dt);
    // Render
    SDL_SetRenderDrawColor(renderer, 62, 44, 33, 255);
    SDL_RenderClear(renderer);
    current_state->draw(current_state->data);
    draw_fps();
    SDL_RenderPresent(renderer);
#ifdef DEBUG
    reload_sprites();
#endif
}

int main(UNUSED int argc, UNUSED char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("Clara Stirzaker and the Crypt of Time",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        (s32)window_w,(s32)window_h,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(window == 0){
    }
    
    renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(renderer == 0){
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    sound_init();

    if(TTF_Init() != 0){
        printf("Could not init font\n");
    }
    hud_font  = TTF_OpenFont("data/font.ttf", 24);
    menu_font = TTF_OpenFont("data/Adventure.ttf", 64);

#ifdef DEBUG
    editor_state = create_editor_state();
#endif
    game_state   = create_game_state();
    current_state   = &game_state;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_callback, 0, 0, 1);
#else
    while (!GAME_QUIT) {
        main_callback(0);
    }
#endif

    sound_quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
