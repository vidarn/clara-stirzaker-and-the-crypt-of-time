#include "main.h"
#include "game.h"
#include "sprite.h"
#include "editor.h"
#include "assets.h"
#include "stretchy_buffer.h"
#include "khash.h"

enum{
    OBJ_ALIVE   = (1 << 0),
    OBJ_VISIBLE = (1 << 1),
    OBJ_SOLID   = (1 << 2),
    OBJ_DEFAULT = OBJ_ALIVE|OBJ_VISIBLE|OBJ_SOLID,
};
typedef struct
{
    void (*update)(Object *obj, GameData *gd, float dt);
    void (*turn)(Object *obj, GameData *gd);
    void (*free)(Object *obj);
    void (*on_switch)(Object *obj, GameData *gd);
}ObjectFuncs;
enum{
    OBJ_player,
    OBJ_wall,
    OBJ_wall_switch,
};
enum{
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT,
};
struct Object
{
    void *data;
    u32 type;
    uvec2 pos;
    u32 sprite;
    u8 flags;
    u8 direction; // 0 -up 1 -right 2-down 3-left
    uvec2 prev_pos;
    vec2 offset;
    u32 index;
};

//TODO(Vidar): move this to GameData
static u32 map_size_x = 8;
static u32 map_size_y = 6;

enum{
    SWITCH_A = 1,
    SWITCH_B = 2,
};

struct GameData
{
    float turn_time;
    u8 switches;
    float time;
    u32 player_id;
    Camera camera;
    Object *objs;
    u32 *map;
    s32 *obj_map; // store the obj index at each tile
};

u32 add_obj(Object obj, GameData *gd)
{
    obj.prev_pos = obj.pos;
    size_t count = sb_count(gd->objs);
    u8 found = 0;
    for(int i=0;i<count;i++){
        if(!(gd->objs[i].flags & OBJ_ALIVE)){
            obj.index = i;
            gd->objs[i] = obj;
            found = 1;
            break;
        }
    }
    if(!found){
        obj.index = count;
        sb_push(gd->objs,obj);
    }
    if(obj.flags & OBJ_SOLID){
        u32 a = obj.pos.x + obj.pos.y*map_size_x;
        assert(gd->obj_map[a] == -1);
        gd->obj_map[a] = obj.index;
    }
    return obj.index;
}


void move_obj(Object *obj, float x, float y, GameData *gd);
u8 is_valid_location(u32 x, u32 y, GameData *gd)
{
    if(x>=map_size_x || y >= map_size_y){
        return 0;
    }
    u32 i = gd->obj_map[x+y*map_size_x];
    if(i != -1){
        if(gd->objs[i].flags & OBJ_SOLID){
            return 0;
        }
    }
    return 1;
    //return gd->map[x+y*map_size_x] == 0;
}
Object *get_obj_at_location(u32 x, u32 y, GameData *gd)
{
    Object *obj = 0;
    u32 index = gd->obj_map[x+y*map_size_x];
    if(index != -1){
        obj = gd->objs + index;
    }
    return obj;
}


void spawn_hero(u32 x, u32 y, GameData *gd)
{
    Object obj = {0,OBJ_player,{(float)x,(float)y},SPRITE_hero,
        OBJ_DEFAULT,0};
    gd->player_id = add_obj(obj,gd);
}
void spawn_wall(u32 x, u32 y, GameData *gd)
{
    Object obj = {0,OBJ_wall,{(float)x,(float)y},SPRITE_wall,
        OBJ_DEFAULT};
    add_obj(obj,gd);
}
 
typedef struct {
    u8 phase;
    u8 switch_id;
}Switchable;

void spawn_wall_switch(u8 phase, u8 switch_id, u32 sprite, u32 x, u32 y, GameData *gd)
{
    Switchable* s = malloc(sizeof(Switchable));
    s->phase = phase;
    s->switch_id = switch_id;
    Object obj = {s,OBJ_wall_switch,{(float)x,(float)y},sprite,
        OBJ_DEFAULT};
    add_obj(obj,gd);
}
void spawn_wall_switch_A(u32 x, u32 y, GameData *gd)
{
    spawn_wall_switch(0,SWITCH_A,SPRITE_wall_switch_A,x,y,gd);
}
void spawn_wall_switch_A_2(u32 x, u32 y, GameData *gd)
{
    spawn_wall_switch(1,SWITCH_A,SPRITE_wall_switch_A,x,y,gd);
}
void spawn_wall_switch_B(u32 x, u32 y, GameData *gd)
{
    spawn_wall_switch(0,SWITCH_B,SPRITE_wall_switch_B,x,y,gd);
}
void spawn_wall_switch_B_2(u32 x, u32 y, GameData *gd)
{
    spawn_wall_switch(1,SWITCH_B,SPRITE_wall_switch_B,x,y,gd);
}
void update_wall_switch(Object *obj, GameData *gd, float dt)
{
    Switchable *s = (Switchable*)obj->data;
    //TODO(Vidar): Handle the case when the player is in the same tile as the wall
    u32 a = obj->pos.x + obj->pos.y*map_size_x;
    if((gd->switches & s->switch_id) == s->phase*s->switch_id && !(obj->flags & OBJ_VISIBLE)){
        if(is_valid_location(obj->pos.x,obj->pos.y,gd)){
            obj->flags |= OBJ_VISIBLE;
            obj->flags |= OBJ_SOLID;
            gd->obj_map[a] = obj->index;
            gd->map[a] = 0;
        }
    }
    if((gd->switches & s->switch_id) != s->phase*s->switch_id && (obj->flags & OBJ_VISIBLE)){
        obj->flags &= ~OBJ_VISIBLE;
        obj->flags &= ~OBJ_SOLID;
        gd->obj_map[a] = -1;
        gd->map[a] = s->switch_id == SWITCH_A ? 1 : 2;
    }
}
void free_wall_switch(Object *obj)
{
    free(obj->data);
}

ObjectFuncs obj_funcs[] = {
    {0,0,0,0},
    {0,0,0,0},
    {update_wall_switch,0,free_wall_switch,0},
};
void load_level(const char *filename, GameData *gd);

void move_obj(Object *obj, float x, float y, GameData *gd)
{
    u32 a = obj->pos.x + obj->pos.y*map_size_x;
    gd->obj_map[a] = -1;

    obj->prev_pos = obj->pos;
    obj->pos.x += x;
    obj->pos.y += y;

    a = obj->pos.x + obj->pos.y*map_size_x;
    assert(gd->obj_map[a] == -1);
    gd->obj_map[a] = obj->index;
}

static void move_player(GameData *gd,float x, float y)
{
    Object *player = gd->objs + gd->player_id;
    u32 target_x = player->pos.x+x;
    u32 target_y = player->pos.y+y;
    if(is_valid_location(target_x, target_y,gd)){
        u8 valid_move = 1;
        Object *other = get_obj_at_location(target_x,target_y,gd);
        if(valid_move){
            gd->turn_time = 0.f;
            add_tweener(&(gd->turn_time),1,200,TWEEN_CUBICEASEINOUT);
            move_obj(player, x,y, gd);
            for(int i=0;i<sb_count(gd->objs);i++){
                Object *obj = gd->objs + i;
                if(obj->flags & OBJ_ALIVE){
                    if(obj_funcs[obj->type].turn){
                        obj_funcs[obj->type].turn(obj,gd);
                    }
                }
            }
        }
    }
}

static void flip_switch(u8 s, GameData *gd)
{
    gd->switches ^= s;
    for(int i=0;i<sb_count(gd->objs);i++){
        Object *obj = gd->objs +i;
        if(obj_funcs[obj->type].on_switch != 0){
            obj_funcs[obj->type].on_switch(obj,gd);
        }
    }
}

static void mouse_down(float x, float y, GameData *gd)
{   
    printf("x:%f y:%f\n",x,y);
    if(x < .5f){
        flip_switch(SWITCH_A,gd);
    }else{
        flip_switch(SWITCH_B,gd);
    }
}

static void input_game(GameStateData *data,SDL_Event event)
{
    GameData *gd = (GameData*)data->data;
    switch(event.type){
            case SDL_KEYDOWN: {
                if(!event.key.repeat && event.key.type == SDL_KEYDOWN){
                    switch(event.key.keysym.sym){
                        case SDLK_RETURN:
                            load_level("data/current.map",gd);
                            break;
                        case SDLK_j:
                            flip_switch(SWITCH_A,gd);
                            break;
                        case SDLK_k:
                            flip_switch(SWITCH_B,gd);
                            break;
                    }
                }
            }break;
            case SDL_FINGERDOWN: {
                float x,y;
                pixels2screen(event.tfinger.x,event.tfinger.y,&x,&y);
                mouse_down(x,y,gd);
            }break;
            case SDL_MOUSEBUTTONDOWN: {
                float x,y;
                pixels2screen(event.button.x,event.button.y,&x,&y);
                mouse_down(x,y,gd);
            }break;
        default: break;
    }
}

static void draw_game(GameStateData *data)
{
    GameData *gd = (GameData*)data->data;
    float w = (float)window_w;
    float h = (float)(window_h-80);
    gd->camera.scale = w/(float)tile_w/(float)map_size_x;
    gd->camera.scale = min(h/(float)tile_h/(float)(map_size_y+1),
            gd->camera.scale);
    float a = max(w,h);
    gd->camera.offset_x = 0.5f*(w
            -gd->camera.scale*(float)tile_w*(float)map_size_x);
    gd->camera.offset_y = 0.5f*(h
            -gd->camera.scale*(float)tile_h*(float)(map_size_y-1));
    u32 tile;
    for(int y=0;y<map_size_y;y++){
        for(int x=0;x<map_size_x;x++){
            tile = tiles[gd->map[x+y*map_size_x]].sprite;
            draw_sprite_at_tile(tile, x, y, gd->camera);
        }
    }
    for(int y=0;y<map_size_y;y++){
        for(int x=0;x<map_size_x;x++){
            u32 i = gd->obj_map[x+y*map_size_x];
            if(i!=-1){
                Object *obj = &gd->objs[i];
                if(obj->flags & (OBJ_VISIBLE)){
                    float x = gd->turn_time*obj->pos.x
                        + (1.f-gd->turn_time)*obj->prev_pos.x+obj->offset.x;
                    float y = gd->turn_time*obj->pos.y
                        + (1.f-gd->turn_time)*obj->prev_pos.y+obj->offset.y;
                    if(obj->pos.x != obj->prev_pos.x || obj->pos.y != obj->prev_pos.y){
                        y -= sin(gd->turn_time*M_PI)*0.3;
                    }
                    draw_sprite_at_tile(obj->sprite, x, y, gd->camera);
                }
            }
        }
    }
    // Draw buttons
    Camera cam = {};
    cam.scale = 0.7f;
    u32 x,y;
    screen2pixels(0.5f,0.f,&x,&y);
    draw_sprite(SPRITE_tile_A, 0, h/cam.scale, cam);
    draw_sprite(SPRITE_tile_B, (w)/cam.scale-sprite_size[SPRITE_tile_B*2], h/cam.scale, cam);
}

static void get_direction_xy(u8 dir, float *x, float *y)
{
    *x = 0;
    *y = 0;
    switch(dir){
        case DIR_UP:
            *y = -1; break;
        case DIR_DOWN:
            *y =  1; break;
        case DIR_LEFT:
            *x = -1; break;
        case DIR_RIGHT:
            *x =  1; break;
    }
}

static void update_game(GameStateData *data,float dt)
{
    GameData *gd = (GameData*)data->data;
    gd->time += dt; //NOTE(Vidar): I know this is stupid, should be fixed point...
    if(gd->turn_time > 1.f - 0.0001f){
        for(int i=0;i<sb_count(gd->objs);i++){
            Object *obj = gd->objs + i;
            obj->prev_pos = obj->pos;
        }
        float x = 0;
        float y = 0;
        Object *player = &gd->objs[gd->player_id];
        get_direction_xy(player->direction,&x,&y);
        u32 target_x = player->pos.x+x;
        u32 target_y = player->pos.y+y;
        if(!is_valid_location(target_x,target_y,gd)){
            player->direction = (player->direction + 1) % 4;
            get_direction_xy(player->direction,&x,&y);
            target_x = player->pos.x+x;
            target_y = player->pos.y+y;
        }
        if(!is_valid_location(target_x,target_y,gd)){
            player->direction = (player->direction + 2) % 4;
            get_direction_xy(player->direction,&x,&y);
            target_x = player->pos.x+x;
            target_y = player->pos.y+y;
        }
        if(!is_valid_location(target_x,target_y,gd)){
            player->direction = (player->direction + 3) % 4;
            get_direction_xy(player->direction,&x,&y);
            target_x = player->pos.x+x;
            target_y = player->pos.y+y;
        }
        move_player(gd,x,y);
    }
    for(int i=0;i<sb_count(gd->objs);i++){
        Object *obj = gd->objs + i;
        if(obj->flags & OBJ_ALIVE){
            if(obj_funcs[obj->type].update){
                obj_funcs[obj->type].update(obj,gd,dt);
            }
        }
    }
}

void load_level(const char *filename, GameData *gd)
{
    gd->turn_time = 1.f;
    gd->time = 0;
    gd->player_id = 0;
    gd->switches = 0;
    for(int i=0;i<sb_count(gd->objs);i++){
        Object *obj = gd->objs + i;
        if(obj->flags & OBJ_ALIVE){
            if(obj_funcs[obj->type].free){
                obj_funcs[obj->type].free(obj);
            }
            obj->flags = 0;
        }
    }
    Map m = load_map(filename);
    map_size_x = m.w;
    map_size_y = m.h;
    if(gd->map){
        free(gd->map);
    }
    if(gd->obj_map){
        free(gd->obj_map);
    }
    gd->map = malloc(sizeof(u32)*m.w*m.h);
    gd->obj_map = malloc(sizeof(s32)*m.w*m.h);
    //layer 0 - tiles
    for(int y=0;y<m.h;y++){
        for(int x=0;x<m.w;x++){
            gd->map[x+y*m.w] = m.tiles[x+y*m.w];
            gd->obj_map[x+y*m.w] = -1;
        } 
    }
    //layer 1 - spawners
    s16 *t = m.tiles + m.w*m.h;
    for(int y=0;y<m.h;y++){
        for(int x=0;x<m.w;x++){
            s16 s = t[x+y*m.w];
            if(s != -1){
                Spawner spawner = spawners[s-num_tiles];
                spawner.spawn_func(x,y,gd);
            }
        }
    }
    delete_map(m);
}

GameState create_game_state() {
    GameStateData *data = malloc(sizeof(GameStateData));
    memset(data,0,sizeof(GameStateData));
    GameData *gd = malloc(sizeof(GameData));
    memset(gd,0,sizeof(GameData));
    FILE *f = fopen("data/hero.png","rb");
    s32 w,h,c;
    load_sprites();
    load_level("data/current.map",gd);
    data->data = gd;
    GameState game_state = {input_game, draw_game,update_game,data,0,0};
    return  game_state;
}



