#include "main.h"
#include "game.h"
#include "sprite.h"
#include "sound.h"
#include "editor.h"
#include "assets.h"
#include "stretchy_buffer.h"
#include "sound.h"

typedef struct
{
    const char *filename;
    const char *name;
    const char **message;
    const u32 message_lines;
}Level;

#define LEVEL_LIST \
LEVEL(level0,"- The crypt of time","Welcome to the crypt of time!"\
        ,""\
        ,"Clara will move around the tomb on her own."\
        ,""\
        ,"When she encounters a wall she will turn"\
        ,"clockwise and keep moving.")\
LEVEL(level1,"- The moving walls",\
        "Press 'j' on the keyboard or click/touch the left"\
        ,"half of the screen to activate the blue pillars."\
        ,""\
        ,""\
        "Press 'k' or click/touch the right"\
        ,"half of the screen to activate the red pillars."\
        ,""\
        ,"Press 'r' on the keyboard to restart the level"\
        ,"if you get stuck.")\
LEVEL(level2,"- The curse of aging",\
        "Time passes at an increased rate in the crypt.",\
        "Use the mystical power in the hourglass to",\
        "reverse the aging")\
LEVEL(level3,"- The key of the Nile",\
        "Sometimes the way forward is blocked",\
        "Collect all artifacts to open the door")\
LEVEL(skeleton1,"- The rattle of bones"\
        ,"A guardian of the tomb awakens."\
        ,""\
        ,"Skeletons are dangerous but stupid"\
        ,"use the layout of the crypt to trap it.")\
LEVEL(skeleton2,"- The old swithceroo")\
LEVEL(fountain,"- The fountain of youth")\
LEVEL(catacomb,"- The catacombs")\
LEVEL(twist,"- The crossroads")\
LEVEL(trapped,"- The trap")\
LEVEL(maze,"- The maze of madness")\
LEVEL(last,"- The crystal of time",\
        "Clara has found the crystal of time",\
        "and has been granted ultimate power",\
        "",\
        "Thanks for playing!")\
LEVEL(current,"- Game over")\

#define LEVEL(file,title,...) const char *message_##file[] = {__VA_ARGS__};
    LEVEL_LIST
#undef LEVEL

static const Level levels[] = {
#define LEVEL(file,title,...) {"data/" #file ".map", title, message_##file,\
    sizeof(message_##file)/sizeof(*message_##file)},
    LEVEL_LIST
#undef LEVEL
};

const u32 lives_per_age[] = {
    2, 2, 2, 2
};
const u32 turn_time_per_age[] = {
    300, 350, 350, 600
};
const u32 sprite_per_age[] = {
    SPRITE_hero, SPRITE_hero2, SPRITE_hero3, SPRITE_hero4,
};
const u32 hurt_per_age[] = {
    SOUND_hurt1, SOUND_hurt2, SOUND_hurt3, SOUND_hurt4,
};
const float age_time[] = {
    0.f, 8.f, 17.f, 40.f
};
const float max_age = 60.f;
const u32 start_years = 12;
const u32 end_years = 105;
const u32 num_ages = array_count(age_time);
const float normal_age_rate = 1.5f;

static const u8 num_levels = sizeof(levels)/sizeof(*levels);
static u32 turn_time = 350;
static const u32 message_time = 700;
static char level_text[32];
static const char *level_name;
enum{
    OBJ_ALIVE   = (1 << 0),
    OBJ_VISIBLE = (1 << 1),
    OBJ_SOLID   = (1 << 2),
    OBJ_DEFAULT = OBJ_ALIVE|OBJ_VISIBLE|OBJ_SOLID, OBJ_MOVING  = (1 << 3),
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
    OBJ_hourglass,
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
    STATE_LEVEL_START,
};
struct GameData
{
    float turn_time;
    float blink_time;
    float message_time;
    float age_time;
    float age_rate;
    u8 switches;
    u8 next_switch;
    u8 lives;
    u8 age;
    u8 state;
    u8 current_level;
    u8 init;
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
        OBJ_SOLID|OBJ_VISIBLE|OBJ_ALIVE|OBJ_MOVING,0};
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
void spawn_crystal(u32 x, u32 y, GameData *gd)
{
    Object obj = {0,OBJ_hourglass,{(float)x,(float)y},SPRITE_crystal,
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
    u32 index = add_obj(obj,gd);
    u32 a = obj.pos.x + obj.pos.y*map_size_x;
    gd->obj_map[a] = index;
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
        play_sound(hurt_per_age[gd->age],1.f);
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
    {
        Object obj = {d,OBJ_gate,{(float)(x-1),(float)y},SPRITE_gate_closed,
            OBJ_ALIVE};
        u32 a = obj.pos.x + obj.pos.y*map_size_x;
        gd->obj_map[a] = add_obj(obj,gd);
    }
}
void free_gate(Object *obj)
{
    free(obj->data);
}
u8 is_solid_gate(Object *a, Object *b, GameData *gd)
{
    GateData *d = a->data;
    return d->open == 0 || b->type != OBJ_player;
}
u8 collide_gate(Object *a, Object *b, GameData *gd)
{
    GateData *d = a->data;
    if(d->open && b->type == OBJ_player){
        if(levels[gd->current_level].message != message_level1 &&
          levels[gd->current_level].message != message_level0){
            gd->state = STATE_LEVEL_FINISHED;
            gd->message_time = 0.f;
            add_tweener(&gd->message_time,1.f,message_time,TWEEN_ELASTICEASEOUT);
        }else{
            gd->current_level++;
            load_level(gd);
        }
        play_sound(SOUND_bell,1.f);
    }
    return 0;
}
void update_gate(Object *obj, GameData *gd, float dt)
{
    GateData *d = obj->data;
    if(!d->open){
        u8 ankh = 0;
        for(int i=0;i<sb_count(gd->objs);i++){
            Object *obj = gd->objs+i;
            if(obj->type == OBJ_ankh && obj->flags & OBJ_ALIVE){
                ankh = 1;
            }
        }
        if(!ankh){
            d->open = 1;
            obj->sprite = SPRITE_gate;
        }
    }
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
    return b->type != OBJ_player;
}
u8 collide_ankh(Object *a, Object *b, GameData *gd)
{
    if(b->type == OBJ_player){
        for(int i=0;i<sb_count(gd->objs);i++){
            Object *obj = gd->objs+i;
            if(obj->type == OBJ_gate){
                destroy_obj(a->index,gd);
            }
        }
        play_sound(SOUND_chime,1.f);
    }
    return 0;
}

void spawn_hourglass(u32 x, u32 y, GameData *gd)
{
    Object obj = {0,OBJ_hourglass,{(float)x,(float)y},SPRITE_hourglass,
        OBJ_VISIBLE|OBJ_ALIVE};
    u32 a = obj.pos.x + obj.pos.y*map_size_x;
    gd->obj_map[a] = add_obj(obj,gd);
}
void update_hourglass(Object *obj, GameData *gd, float dt)
{
    obj->offset.y = sin(gd->time*2.f)*.2f;
}
u8 is_solid_hourglass(Object *a, Object *b, GameData *gd)
{
    return b->type != OBJ_player;
}
u8 collide_hourglass(Object *a, Object *b, GameData *gd)
{
    if(b->type == OBJ_player){
        gd->age_rate = -6.f*normal_age_rate;
        add_tweener(&gd->age_rate,normal_age_rate,2800,TWEEN_LINEARINTERPOLATION);
        destroy_obj(a->index,gd);
        play_sound(SOUND_chime,1.f);
    }
    return 0;
}

ObjectFuncs obj_funcs[] = {
    {update_hero,0,0,0},
    {0,0,0,0},
    {update_wall_switch,0,free_wall_switch,0},
    {update_skeleton,is_solid_skeleton,0,collide_skeleton},
    {update_gate,is_solid_gate,0,collide_gate},
    {update_ankh,is_solid_ankh,0,collide_ankh},
    {update_hourglass,is_solid_hourglass,0,collide_hourglass},
};

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
            gd->next_switch = gd->switches ^ s;
            break;
        case STATE_LEVEL_FINISHED:
            if(gd->message_time > 0.999f){
                gd->current_level = min(num_levels-1,gd->current_level+1);
                load_level(gd);
            }
            break;
        case STATE_LEVEL_START:
            if(gd->message_time > 0.999f){
                gd->state = STATE_NORMAL;
            }
            break;
        case STATE_DEAD:
            if(gd->message_time > 0.999f){
                load_level(gd);
            }
            break;
    }
}

static void mouse_down(float x, float y, GameData *gd)
{   
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
#ifdef DEBUG
                        case SDLK_PLUS:
                        case SDLK_EQUALS:
                            gd->current_level = min(num_levels-1,gd->current_level+1);
                            load_level(gd);
                            break;
                        case SDLK_MINUS:
                            gd->current_level = max(0,gd->current_level-1);
                            load_level(gd);
                            break;
                        case SDLK_a:
                            turn_time = 100;
                            break;
                        case SDLK_d:
                            turn_time = 350;
                            break;
#endif

                        case SDLK_r:
                            load_level(gd);
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
    Camera cam = {};
    cam.scale = (float)window_w/2000.f;

    float top_bar_height = 180.f*cam.scale;
    float bottom_bar_height = 10.f;//150.f*cam.scale;
    float w = (float)window_w;
    float h = (float)(window_h-top_bar_height-bottom_bar_height);

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
                        + (1.f-gd->turn_time)*obj->prev_pos.x;
                    float y = gd->turn_time*obj->pos.y
                        + (1.f-gd->turn_time)*obj->prev_pos.y;
                    draw_sprite_at_tile(SPRITE_shadow, x, y, gd->camera);
                    if(obj->pos.x != obj->prev_pos.x || obj->pos.y != obj->prev_pos.y){
                        y -= sin(gd->turn_time*M_PI)*0.3;
                    }
                    x +=obj->offset.x;
                    y +=obj->offset.y;
                    if(obj->type == OBJ_player){
                        float alpha = 1.f;
                        u32 sprite = sprite_per_age[gd->age];
                        u32 older_sprite = sprite_per_age[gd->age];
                        float age_fraction = 0.f;
                        if(gd->age+1 < num_ages){
                            older_sprite = sprite_per_age[gd->age+1];
                            age_fraction = (gd->age_time - age_time[gd->age])/
                                (age_time[gd->age+1] - age_time[gd->age]);
                            float low = 0.8f;
                            age_fraction = (max(age_fraction, low)-low)/(1.f-low);
                        }
                        if(gd->blink_time > 0.1f){
                            alpha = .6f + 0.3f*sin(gd->time*13.f);
                        }
                        draw_sprite_at_tile_with_alpha(sprite, x, y,
                                gd->camera,alpha*(1.f-age_fraction));
                        draw_sprite_at_tile_with_alpha(older_sprite, x, y,
                                gd->camera,alpha*age_fraction);
                    } else {
                        draw_sprite_at_tile(obj->sprite, x, y, gd->camera);
                    }
                }
            }
        }
    }

    // Draw borders
    u32 x,y;
    screen2pixels(0.5f,0.f,&x,&y);
    float border = 180.f*cam.scale;
    /*draw_sprite(SPRITE_tile_A, 0, (h+top_bar_height)/cam.scale, cam);
    draw_sprite(SPRITE_tile_B, (w)/cam.scale-sprite_size[SPRITE_tile_B*2],
            (h+top_bar_height)/cam.scale, cam);*/
    //draw_sprite_rect(SPRITE_buttons, 0, (h+top_bar_height)/cam.scale,window_w,window_h);
    /*float button_border = 280.f*cam.scale;
    draw_sprite_rect(SPRITE_buttons, button_border, h+top_bar_height,
            window_w-button_border,window_h);*/
    draw_sprite_rect(SPRITE_border_A, 0, 0,
            border,border);
    draw_sprite_rect(SPRITE_border_B, window_w-border, 0,
            window_w,border);
    draw_sprite_rect(SPRITE_border_A2, 0, window_h-border,
            border,window_h);
    draw_sprite_rect(SPRITE_border_B2, window_w-border, window_h-border,
            window_w,window_h);

    // Draw hud
    for(int i=0;i<lives_per_age[gd->age];i++){
        if(gd->lives > i){
            draw_sprite(SPRITE_heart, border*0.5f+i*sprite_size[SPRITE_heart*2],
                    sprite_size[SPRITE_heart*2+1]*.5f, cam);
        }else{
            draw_sprite(SPRITE_heart_empty, border*0.5f+i*sprite_size[SPRITE_heart*2],
                    sprite_size[SPRITE_heart*2+1]*.5f, cam);
        }
    }
    static char buffer[64] = {};
    if(levels[gd->current_level].message != message_level1 &&
      levels[gd->current_level].message != message_level0){
        SDL_Color font_color = {0,0,0,255};
        sprintf(buffer,"Age: %d",(u32)((float)start_years
                    + (float)(end_years - start_years)*(gd->age_time/max_age)));
        draw_text(menu_font,font_color,buffer, x, 0.f,.5f,-.3f,cam.scale*1.5f);
    }
    screen2pixels(1.f,0.f,&x,&y);
    {
        SDL_Color font_color = {159,75,33,255};
        draw_text(menu_font,font_color,level_text,x,y,1.5f,0.f,cam.scale*1.5f);
    }
    {
        SDL_Color font_color = {181,108,16,255};
        draw_text(menu_font,font_color,level_name,x,y,1.5f,-1.6f,cam.scale*.7f);
    }
    static const char *dead_message[] = {
        "You are dead!",
        "",
        "Try again",
    };
    switch(gd->state){
        case STATE_DEAD:
            draw_text_dialog(dead_message,
                    sizeof(dead_message)/sizeof(*dead_message),
                    cam.scale*gd->message_time);
            break;
        case STATE_LEVEL_FINISHED:
        {
            static const char *level_finished_message[] = {
                "You finished the level",
                "",
            };
            sprintf(buffer,"at an age of %d years",
                    (u32)((float)start_years + (float)(end_years - start_years)
                        *(gd->age_time/max_age)));
            level_finished_message[1] = buffer;
            draw_text_dialog(level_finished_message,
                sizeof(level_finished_message)/sizeof(*level_finished_message),
                cam.scale*gd->message_time);
            break;
        }
        case STATE_LEVEL_START:
            if(levels[gd->current_level].message_lines > 0){
                draw_text_dialog(levels[gd->current_level].message,
                    levels[gd->current_level].message_lines,
                    cam.scale*gd->message_time);
            }
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
    if(gd->init == 0){
        load_level(gd);
        gd->init = 1;
    }
    gd->time += dt; //NOTE(Vidar): I know this is stupid, should be fixed point...
    u8 moved = 0;
    switch(gd->state){
    case STATE_NORMAL:
        // NOTE(Vidar): No aging at level 1
        if(levels[gd->current_level].message != message_level1 &&
           levels[gd->current_level].message != message_level0 &&
           levels[gd->current_level].message != message_last){
            gd->age_time += dt*gd->age_rate;
            gd->age_time = max(min(gd->age_time,max_age),0.f);
            for(int i =0;i<num_ages;i++){
                if(gd->age_time > age_time[i]){
                    gd->age = i;
                }
            }
        }
        turn_time = turn_time_per_age[gd->age];
        if(gd->lives <= 0 || gd->age_time >= max_age){
            gd->state = STATE_DEAD;
            gd->message_time = 0.2f;
            add_tweener(&gd->message_time,1.f,message_time,TWEEN_ELASTICEASEOUT);
            //play_sound(SOUND_death,1.f);
        }
        if(gd->turn_time > 1.f - 0.0001f){
            gd->turn_time = 0.f;
            add_tweener(&(gd->turn_time),1,turn_time,TWEEN_CUBICEASEINOUT);

            if(gd->switches != gd->next_switch){
                play_sound(SOUND_slide,1.f);
            }
            gd->switches = gd->next_switch;
            for(int i=0;i<sb_count(gd->objs);i++){
                Object *obj = gd->objs + i;
                if(obj->flags & OBJ_ALIVE){
                    if(obj->type & OBJ_wall_switch){
                        obj_funcs[obj->type].update(obj,gd,dt);
                    }
                }
            }

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
                        moved = 1;
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
    if(moved){
        play_sound(SOUND_step,0.4f);
    }
}

void load_level(GameData *gd)
{
    gd->turn_time = 1.f;
    gd->time = 0;
    gd->player_id = 0;
    gd->switches = 0;
    gd->next_switch = 0;
    gd->lives = lives_per_age[gd->age];
    gd->state = STATE_NORMAL;
    gd->age_rate = normal_age_rate;
    gd->age_time = age_time[1];
    gd->age = 1;
    if(levels[gd->current_level].message_lines > 0){
        gd->state = STATE_LEVEL_START;
    }
    gd->message_time = 0.f;
    add_tweener(&gd->message_time,1.f,message_time,TWEEN_ELASTICEASEOUT);
    for(int i=0;i<sb_count(gd->objs);i++){
        Object *obj = gd->objs + i;
        if(obj->flags & OBJ_ALIVE){
            if(obj_funcs[obj->type].free){
                obj_funcs[obj->type].free(obj);
            }
            obj->flags = 0;
        }
    }
    Map m = load_map(levels[gd->current_level].filename);
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
    level_name = levels[gd->current_level].name;
}

GameState create_game_state() {
    GameStateData *data = malloc(sizeof(GameStateData));
    memset(data,0,sizeof(GameStateData));
    GameData *gd = malloc(sizeof(GameData));
    memset(gd,0,sizeof(GameData));
    load_sprites();
    data->data = gd;
    GameState game_state = {input_game, draw_game,update_game,data,0,0};
    play_music();
    return  game_state;
}



