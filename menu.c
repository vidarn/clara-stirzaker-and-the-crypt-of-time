#include "main.h"

typedef struct
{
    int selection;
}MenuData;

static const char *menu_entries[] = {
    "New game",
    "Level editor",
    "Exit",
};
static const int num_menu_entries = sizeof(menu_entries)/sizeof(*menu_entries);
static float menu_entry_size[num_menu_entries] = {};
static const float unselected_size = 0.7f;

static void input_menu(GameStateData *data,SDL_Event event)
{
    MenuData *md = (MenuData*)data->data;
    switch(event.type){
        case SDL_KEYDOWN: {
            if(!event.key.repeat && event.key.type == SDL_KEYDOWN){
                switch(event.key.keysym.sym){
                    case SDLK_DOWN:
                        add_tweener(menu_entry_size+md->selection,
                                unselected_size,600,TWEEN_ELASTICEASEOUT);
                        md->selection = (md->selection + 1) % num_menu_entries;
                        add_tweener(menu_entry_size+md->selection,1.f,600,
                                TWEEN_ELASTICEASEOUT);
                        break;
                    case SDLK_UP:
                        add_tweener(menu_entry_size+md->selection,
                                unselected_size,600,TWEEN_ELASTICEASEOUT);
                        md->selection = md->selection - 1;
                        if(md->selection < 0) md->selection = num_menu_entries-1;
                        add_tweener(menu_entry_size+md->selection,1.f,600,
                                TWEEN_ELASTICEASEOUT);
                        break;
                }
            }
        }break;
        default: break;
    }
}

static void draw_menu(GameStateData *data)
{
    MenuData *md = (MenuData*)data->data;
    int ystep = 60;
    SDL_Color font_color = {0,0,0,255};
    u32 x,y;
    for(int i=0;i<num_menu_entries;i++){
        screen2pixels(0.5f,(float)(i+1)/(float)(num_menu_entries+1),&x,&y);
        draw_text(menu_font,font_color,menu_entries[i],x,y,0.5f,0.5f,
                menu_entry_size[i]);
    }
}

static void update_menu(GameStateData *data,float dt)
{
    MenuData *md = (MenuData*)data->data;
}

GameState create_menu_state() {
    GameStateData *data = malloc(sizeof(GameStateData));
    memset(data,0,sizeof(GameStateData));
    MenuData *md = malloc(sizeof(MenuData));
    memset(md,0,sizeof(MenuData));
    md->selection = 0;
    for(int i=0;i<num_menu_entries;i++){
        float s = md->selection == i ? 1.f: unselected_size;
        //add_tweener(menu_entry_size+i,s,1000,TWEEN_ELASTICEASEOUT);
        menu_entry_size[i] = s;
    }
    data->data = md;
    GameState menu_state = {input_menu, draw_menu,update_menu,data,0,0};
    return  menu_state;
}


