#include "main.h"
#include "game.h"
#include "sprite.h"
#include "editor.h"
#include "assets.h"
#include "stretchy_buffer.h"
#include "khash.h"

static const char *level_list[] = {
    //"data/current.map",
    "data/level1.map",
    "data/level2.map",
};
static const char *level_names[] = {
    //"- Working level",
    "- First steps",
    "- Unfinished",
};
static const u8 num_levels = sizeof(level_list)/sizeof(*level_list);
static const u8 num_level_names = sizeof(level_names)/sizeof(*level_names);
static const u32 turn_time = 350;
//static const u32 turn_time = 100;
static const u32 max_lives = 4;
static char level_text[32];
static const char *level_name;
enum{
    OBJ_ALIVE   = (1 << 0),
    OBJ_VISIBLE = (1 << 1),
    OBJ_SOLID   = (1 << 2),
    OBJ_DEFAULT = OBJ_ALIVE|OBJ_VISIBLE|OBJ_SOLID,
    OBJ_MOVING  = (1 << 3),
};
typedef struct
{
    void (*update)(Object *obj, GameData *gd, float dt);
    u8 (*is_solid)(Object *a, Object *b, GameData *gd);
    void (*free)(Object *obj);
    u8 (*collision)(Object *a, Object *b, GameData *gd);
}ObjectFuncs;
ObjectFuncs obj_funcs[];
enum{
    OBJ_player,
    OBJ_wall,
    OBJ_wall_switch,
    OBJ_skeleton,
    OBJ_gate,
    OBJ_ankh,
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

enum{
    STATE_NORMAL,
    STATE_LEVEL_FINISHED,
    STATE_DEAD,
};
struct GameData
{
    float turn_time;
    float blink_time;
    float message_time;
    u8 switches;
    u8 lives;
    u8 state;
    u8 current_level;
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

void destroy_obj(u32 index, GameData *gd)
{
    Object *obj = gd->objs+index;
    if(obj_funcs[obj->type].free){
        obj_funcs[obj->type].free(obj);
    }
    obj->flags = 0;
    for(int i = 0;i < map_size_x*map_size_y;i++){
        if(gd->obj_map[i] == index){
            gd->obj_map[i] = -1;
        }
    }
}

static Object *get_object_at_location(u32 x, u32 y, GameData *gd)
{
    u32 i = gd->obj_map[x+y*map_size_x];
    if(i != -1){
        return &gd->objs[i];
    }
    return 0;
}

void move_obj(Object *obj, float x, float y, GameData *gd);
u8 is_valid_location(u32 x, u32 y, Object *self, GameData *gd)
{
    if(x>=map_size_x || y >= map_size_y){
        return 0;
    }
    Object *obj = get_object_at_location(x,y,gd);
    if(obj && ((obj->flags & OBJ_SOLID) || (self && obj_funcs[obj->type].is_solid && 
        obj_funcs[obj->type].is_solid(obj,self,gd)))){
        return 0;
    }
    return 1;
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
        OBJ_VISIBLE|OBJ_ALIVE|OBJ_MOVING,0};
    gd->player_id = add_obj(obj,gd);
}
void update_hero(Object *obj, GameData *gd, float dt)
{
    if(gd->blink_time > 0.001f){
        obj->flags |= OBJ_SOLID;
    }else{
        obj->flags &= ~OBJ_SOLID;
    }
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
    u32 a = obj->pos.x + obj->pos.y*map_size_x;
    if((gd->switches & s->switch_id) == s->phase*s->switch_id){
        if(is_valid_location(obj->pos.x,obj->pos.y,0,gd)){
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

typedef struct
{
    float stun_time;
}EnemyData;
void spawn_skeleton(u32 x, u32 y, GameData *gd)
{
    EnemyData *d = malloc(sizeof(EnemyData));
    d->stun_time = 0.f;
    Object obj = {d,OBJ_skeleton,{(float)x,(float)y},SPRITE_skeleton,
        OBJ_VISIBLE|OBJ_ALIVE|OBJ_MOVING};
    add_obj(obj,gd);
}
void free_enemy(Object *obj)
{
    free(obj->data);
}
u8 is_solid_skeleton(Object *a, Object *b, GameData *gd)
{
    switch(b->type){
        case OBJ_player:
            return gd->blink_time > 0.001f;
        case OBJ_skeleton:
            return 1;
    }
    return 1;
}
u8 collide_skeleton(Object *a, Object *b, GameData *gd)
{
    if(b->type == OBJ_player && gd->blink_time < 0.001f){
        gd->lives--;
        gd->blink_time = 1.f;
        add_tweener(&gd->blink_time,0.f,10*turn_time,TWEEN_LINEARINTERPOLATION);
        EnemyData *ed = (EnemyData*)a->data;
    }
    return 0;
}
void update_skeleton(Object *obj, GameData *gd, float dt)
{
    EnemyData *ed = (EnemyData*)obj->data;
    if(ed->stun_time > 0.001f){
        obj->flags &= ~OBJ_MOVING;
    }else{
        obj->flags |= OBJ_MOVING;
    }
}

typedef struct{ 
   u8 open; 
}GateData;
void spawn_gate(u32 x, u32 y, GameData *gd)
{
    GateData *d = malloc(sizeof(GateData));
    d->open = 0;
    Object obj = {d,OBJ_gate,{(float)x,(float)y},SPRITE_gate_closed,
        OBJ_VISIBLE|OBJ_ALIVE};
    u32 a = obj.pos.x + obj.pos.y*map_size_x;
    gd->obj_map[a] = add_obj(obj,gd);
}
void free_gate(Object *obj)
{
    free(obj->data);
}
u8 is_solid_gate(Object *a, Object *b, GameData *gd)
{
    return 0;
}
u8 collide_gate(Object *a, Object *b, GameData *gd)
{
    GateData *d = a->data;
    if(d->open){
        gd->state = STATE_LEVEL_FINISHED;
        gd->message_time = 0.f;
        add_tweener(&gd->message_time,1.f,700,TWEEN_ELASTICEASEOUT);
    }
    return 0;
}

void spawn_ankh(u32 x, u32 y, GameData *gd)
{
    Object obj = {0,OBJ_ankh,{(float)x,(float)y},SPRITE_ankh,
        OBJ_VISIBLE|OBJ_ALIVE};
    u32 a = obj.pos.x + obj.pos.y*map_size_x;
    gd->obj_map[a] = add_obj(obj,gd);
}
void update_ankh(Object *obj, GameData *gd, float dt)
{
    obj->offset.y = sin(gd->time*2.f)*.2f;
}
u8 is_solid_ankh(Object *a, Object *b, GameData *gd)
{
    return 0;
}
u8 collide_ankh(Object *a, Object *b, GameData *gd)
{
    if(b->type == OBJ_player){
        for(int i=0;i<sb_count(gd->objs);i++){
            Object *obj = gd->objs+i;
            if(obj->type == OBJ_gate){
                GateData *d = (GateData*)obj->data;
                d->open = 1;
                obj->sprite = SPRITE_gate;
                destroy_obj(a->index,gd);
            }
        }
    }
    return 0;
}

ObjectFuncs obj_funcs[] = {
    {update_hero,0,0,0},
    {0,0,0,0},
    {update_wall_switch,0,free_wall_switch,0},
    {update_skeleton,is_solid_skeleton,0,collide_skeleton},
    {0,is_solid_gate,0,collide_gate},
    {update_ankh,is_solid_ankh,0,collide_ankh},
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
    move_obj(player, x,y, gd);
}

static void flip_switch(u8 s, GameData *gd)
{
    switch(gd->state){
        case STATE_NORMAL:
            gd->switches ^= s;
            break;
        case STATE_LEVEL_FINISHED:
            if(gd->message_time > 0.999f){
                gd->current_level = min(num_levels-1,gd->current_level+1);
                load_level(level_list[gd->current_level],gd);
            }
            break;
        case STATE_DEAD:
            break;
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
                        //DEBUG keys
                        case SDLK_PLUS:
                        case SDLK_EQUALS:
                            gd->current_level = min(num_levels-1,gd->current_level+1);
                            load_level(level_list[gd->current_level],gd);
                            break;
                        case SDLK_MINUS:
                            gd->current_level = max(0,gd->current_level-1);
                            load_level(level_list[gd->current_level],gd);
                            break;
                        case SDLK_RETURN:
                            load_level(level_list[gd->current_level],gd);
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
    float h = (float)(window_h-160);
    float top_bar_height = 80.f;
    gd->camera.scale = w/(float)tile_w/(float)map_size_x;
    gd->camera.scale = min(h/(float)tile_h/(float)(map_size_y+1),
            gd->camera.scale);
    float a = max(w,h);
    gd->camera.offset_x = 0.5f*(w
            -gd->camera.scale*(float)tile_w*(float)map_size_x);
    gd->camera.offset_y = 0.5f*(h
            -gd->camera.scale*(float)tile_h*(float)(map_size_y-1)) + top_bar_height ;
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
                    if(obj->type == OBJ_player && gd->blink_time > 0.1f){
                        draw_sprite_at_tile_with_alpha(obj->sprite, x, y,
                                gd->camera,.6f + 0.3f*sin(gd->time*13.f));
                    } else {
                        draw_sprite_at_tile(obj->sprite, x, y, gd->camera);
                    }
                }
            }
        }
    }
    // Draw buttons
    Camera cam = {};
    cam.scale = 0.7f;
    u32 x,y;
    screen2pixels(0.5f,0.f,&x,&y);
    draw_sprite(SPRITE_tile_A, 0, (h+top_bar_height)/cam.scale, cam);
    draw_sprite(SPRITE_tile_B, (w)/cam.scale-sprite_size[SPRITE_tile_B*2],
            (h+top_bar_height)/cam.scale, cam);
    // Draw hud
    cam.scale = gd->camera.scale*0.6f;
    for(int i=0;i<max_lives;i++){
        if(gd->lives > i){
            draw_sprite(SPRITE_heart, i*sprite_size[SPRITE_heart*2],
                    sprite_size[SPRITE_heart*2+1]*.5f, cam);
        }else{
            draw_sprite(SPRITE_heart_empty, i*sprite_size[SPRITE_heart*2],
                    sprite_size[SPRITE_heart*2+1]*.5f, cam);
        }
    }
    screen2pixels(1.f,0.f,&x,&y);
    {
        SDL_Color font_color = {159,75,33,255};
        draw_text(menu_font,font_color,level_text,x,y,1.5f,0.f,gd->camera.scale*1.5f);
    }
    {
        SDL_Color font_color = {181,108,16,255};
        draw_text(menu_font,font_color,level_name,x,y,1.5f,-1.6f,gd->camera.scale*.7f);
    }
    static const char *level_finished_message[] = {
        "You finished the level",
        "",
        "Good job!",
    };
    switch(gd->state){
        case STATE_DEAD:
            //draw_text_dialog("You are dead, try again", 1, gd->camera.scale);
            break;
        case STATE_LEVEL_FINISHED:
            draw_text_dialog(level_finished_message,
                sizeof(level_finished_message)/sizeof(*level_finished_message),
                gd->camera.scale*gd->message_time);
            break;
    }
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

static void update_direction(Object *obj, GameData *gd)
{
    float x = 0;
    float y = 0;
    get_direction_xy(obj->direction,&x,&y);
    u32 target_x = obj->pos.x+x;
    u32 target_y = obj->pos.y+y;
    if(!is_valid_location(target_x,target_y,obj,gd)){
        obj->direction = (obj->direction + 1) % 4;
        get_direction_xy(obj->direction,&x,&y);
        target_x = obj->pos.x+x;
        target_y = obj->pos.y+y;
    }
    if(!is_valid_location(target_x,target_y,obj,gd)){
        obj->direction = (obj->direction + 2) % 4;
        get_direction_xy(obj->direction,&x,&y);
        target_x = obj->pos.x+x;
        target_y = obj->pos.y+y;
    }
    if(!is_valid_location(target_x,target_y,obj,gd)){
        obj->direction = (obj->direction + 3) % 4;
        get_direction_xy(obj->direction,&x,&y);
        target_x = obj->pos.x+x;
        target_y = obj->pos.y+y;
    }
}

static void update_game(GameStateData *data,float dt)
{
    GameData *gd = (GameData*)data->data;
    gd->time += dt; //NOTE(Vidar): I know this is stupid, should be fixed point...
    if(gd->lives <= 0){
        gd->state = STATE_DEAD;
    }
    switch(gd->state){
    case STATE_NORMAL:
        if(gd->turn_time > 1.f - 0.0001f){
            gd->turn_time = 0.f;
            add_tweener(&(gd->turn_time),1,turn_time,TWEEN_CUBICEASEINOUT);

            for(int i=0;i<sb_count(gd->objs);i++){
                Object *obj = gd->objs + i;
                if(obj->flags & OBJ_MOVING){
                    obj->prev_pos = obj->pos;
                    update_direction(obj,gd);
                    float x = 0;
                    float y = 0;
                    get_direction_xy(obj->direction,&x,&y);
                    u32 target_x = obj->pos.x+x;
                    u32 target_y = obj->pos.y+y;
                    Object *col = get_object_at_location(target_x,target_y,gd);
                    u8 valid = 1;
                    if(col){
                        if(obj_funcs[col->type].collision){
                            valid = valid && obj_funcs[col->type].collision(col,obj,gd);
                        }
                        if(obj_funcs[obj->type].collision){
                            valid = valid && obj_funcs[obj->type].collision(obj,col,gd);
                        }
                    }
                    if(valid && is_valid_location(target_x,target_y,obj,gd)){
                        move_obj(obj,x,y,gd);
                    }
                }
            }
        }
        for(int i=0;i<sb_count(gd->objs);i++){
            Object *obj = gd->objs + i;
            if(obj->flags & OBJ_ALIVE){
                if(obj_funcs[obj->type].update){
                    obj_funcs[obj->type].update(obj,gd,dt);
                }
            }
        }
        break;
    case STATE_LEVEL_FINISHED:
        break;
    case STATE_DEAD:
        break;
    }
}

void load_level(const char *filename, GameData *gd)
{
    gd->turn_time = 1.f;
    gd->time = 0;
    gd->player_id = 0;
    gd->switches = 0;
    gd->lives = max_lives;
    gd->state = STATE_NORMAL;
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
    sprintf(level_text,"Level: %d",gd->current_level+1);
    if(gd->current_level < num_level_names){
       level_name = level_names[gd->current_level];
    }
}

GameState create_game_state() {
    GameStateData *data = malloc(sizeof(GameStateData));
    memset(data,0,sizeof(GameStateData));
    GameData *gd = malloc(sizeof(GameData));
    memset(gd,0,sizeof(GameData));
    FILE *f = fopen("data/hero.png","rb");
    s32 w,h,c;
    load_sprites();
    load_level(level_list[0],gd);
    data->data = gd;
    GameState game_state = {input_game, draw_game,update_game,data,0,0};
    return  game_state;
}



