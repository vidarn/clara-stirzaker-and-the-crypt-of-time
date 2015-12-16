#include "main.h"
#include "map.h"
#include "sprite.h"
#include "assets.h"
#include "editor.h"
#define STB_TILEMAP_EDITOR_IMPLEMENTATION
void STBTE_DRAW_RECT(s32 x0, s32 y0, s32 x1, s32 y1, u32 color);
void STBTE_DRAW_TILE(s32 x0, s32 y0,
              u16 id, s32 highlight, float *data);
#include "stb_tilemap_editor.h"

typedef struct
{
    stbte_tilemap *tilemap;
}EditorData;

static const int gui_size = 2;
static const float tile_scale = 0.5f;
static const Camera camera = {tile_scale,0.f,0.f};
static const u32 map_layers = 2;

enum{
    OVERLAY_SAVE,
    OVERLAY_LOAD,
};
static char overlay_text[256] = "data/current.map";
static u8 overlay_text_visible = 0;
static u8 overlay_first_key = 0;
static u8 overlay_action = 0;

static u32 hotkeys[] = {
   STBTE_act_undo,          SDLK_u,
   STBTE_tool_select,       SDLK_p,
   STBTE_tool_brush,        SDLK_d,
   STBTE_tool_erase,        SDLK_e,
   STBTE_tool_rectangle,    SDLK_r,
   STBTE_tool_eyedropper,   SDLK_i,
   //STBTE_tool_link,         SDLK_l,
   STBTE_act_toggle_grid,   SDLK_g,
   STBTE_act_copy,          SDLK_c,
   STBTE_act_paste,         SDLK_v,
   STBTE_scroll_left,       SDLK_RIGHT,
   STBTE_scroll_right,      SDLK_LEFT,
   STBTE_scroll_up,         SDLK_DOWN,
   STBTE_scroll_down,       SDLK_UP,
};
static u32 num_hotkeys = sizeof(hotkeys)/2/sizeof(u32);

void STBTE_DRAW_RECT(s32 x0, s32 y0, s32 x1, s32 y1, u32 color)
{
    SDL_SetRenderDrawColor(renderer,color>>16,color>>8,color,255);
    SDL_Rect rect = {x0*gui_size,y0*gui_size,
        (x1-x0)*gui_size,(y1-y0)*gui_size};
    SDL_RenderFillRect(renderer,&rect);
}

void STBTE_DRAW_TILE(s32 x0, s32 y0,
              u16 id, s32 highlight, float *data)
{
    u32 sprite = 0;
    if(id < num_tiles){
        sprite = tiles[id].sprite;
    }else{
        sprite = spawners[id-num_tiles].sprite;
    }
    if(data != 0){
        draw_sprite(sprite,x0*gui_size/tile_scale,y0*gui_size/tile_scale,camera);
    } else {
        draw_sprite_clipped(sprite,x0*gui_size/tile_scale,y0*gui_size/tile_scale,camera);
    }
}

static Map get_map(stbte_tilemap *tm)
{
    Map map={};
    stbte_get_dimensions(tm,&map.w,&map.h);
    map.layers = map_layers;
    map.tiles = malloc(map.w*map.h*map.layers*sizeof(s16));
    for(int y=0;y<map.h;y++){
        for(int x=0;x<map.w;x++){
            s16 *tiles = stbte_get_tile(tm, x, y);
            for(int l=0;l<map.layers;l++){
                map.tiles[x+y*map.w+l*map.w*map.h] = tiles[l];
            }
        }
    }
    return map;
}

static void set_map(Map map, stbte_tilemap *tm)
{
    stbte_clear_map(tm);
    stbte_set_dimensions(tm,map.w,map.h);
    assert(map_layers == map.layers);
    //TODO(Vidar): map tile types
    for(int y=0;y<map.h;y++){
        for(int x=0;x<map.w;x++){
            for(int l=0;l<map.layers;l++){
                stbte_set_tile(tm,x,y,l,map.tiles[x+y*map.w+l*map.w*map.h]);
            }
        }
    }
}

static void input_editor(GameStateData *data,SDL_Event event)
{
    EditorData *ed = (EditorData*)data->data;
    switch(event.type){
        case SDL_TEXTINPUT:
        {
            if(overlay_text_visible){
                if(!overlay_first_key){
                    strcat(overlay_text,event.text.text);
                }
                overlay_first_key = 0;
            }
        } break;
        case SDL_KEYDOWN: {
            if(overlay_text_visible){
                switch(event.key.keysym.sym){
                    case SDLK_BACKSPACE:
                        overlay_text[strlen(overlay_text)-1] = 0;
                        break;
                    case SDLK_RETURN:
                        overlay_text_visible = 0;
                        switch(overlay_action){
                            case OVERLAY_SAVE:
                                {
                                    Map map = get_map(ed->tilemap);
                                    save_map(overlay_text,map);
                                    delete_map(map);
                                }
                                break;
                            case OVERLAY_LOAD:
                            {
                                Map m = load_map(overlay_text);
                                if(m.w > 0){
                                    set_map(m,ed->tilemap);
                                }
                                delete_map(m);
                                break;
                            }
                        }
                        break;
                }
            }else{
                for(int i=0;i<num_hotkeys;i++){
                    if(hotkeys[i*2+1] == event.key.keysym.sym){
                        stbte_action(ed->tilemap, hotkeys[i*2]);
                    }
                }
                switch(event.key.keysym.sym){
                    case SDLK_s:
                        overlay_text_visible = 1;
                        overlay_first_key = 1;
                        overlay_action = OVERLAY_SAVE;
                        break;
                    case SDLK_l:
                    {
                        overlay_text_visible = 1;
                        overlay_first_key = 1;
                        overlay_action = OVERLAY_LOAD;
                    } break;
                }
            }
        }
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
        {
            stbte_mouse_sdl(ed->tilemap, &event, 1.f/(float)gui_size,
                    1.f/(float)gui_size, 0, 0);
        } break;
        case SDL_WINDOWEVENT: {
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED){
                u32 window_w = event.window.data1;
                u32 window_h = event.window.data2;
                stbte_set_display(0, 0, window_w/gui_size,
                        window_h/gui_size);
            }
        }break;
    }
}

static void draw_editor(GameStateData *data)
{
    EditorData *ed = (EditorData*)data->data;
    stbte_draw(ed->tilemap);
    if(overlay_text_visible){
        SDL_Color font_color = {0,0,0,255};
        u32 x, y;
        screen2pixels(0.5f,0.5f,&x,&y);
        draw_text(menu_font,font_color,overlay_text,x,y,.5f,.5f,1.f);
    }
}

static void update_editor(GameStateData *data,float dt)
{
    EditorData *ed = (EditorData*)data->data;
    stbte_tick(ed->tilemap, dt);
}

GameState create_editor_state(u32 window_w, u32 window_h) {
    GameStateData *data = malloc(sizeof(GameStateData));
    memset(data,0,sizeof(GameStateData));
    EditorData *ed = malloc(sizeof(EditorData));
    memset(ed,0,sizeof(EditorData));
    stbte_tilemap *tilemap = stbte_create_map(8, 8, map_layers,
            tile_scale*tile_w/gui_size, tile_scale*tile_h/gui_size, 64);
    stbte_set_display(0, 0, window_w/gui_size, window_h/gui_size);
    int a=0;
    for(u32 i=0;i<num_tiles;i++){
        stbte_define_tile(tilemap, a++, 1, "tiles");
    }
    for(u32 i=0;i<num_spawners;i++){
        stbte_define_tile(tilemap, a++, 2, "objects");
    }
    ed->tilemap = tilemap;
    Map m = load_map("data/current.map");
    if(m.w > 0){
        set_map(m,ed->tilemap);
    }
    delete_map(m);
    data->data = ed;
    GameState editor_state = {input_editor, draw_editor, update_editor,data,0,0};
    return  editor_state;
}

