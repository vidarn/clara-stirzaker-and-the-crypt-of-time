#include "sprite.h"
#include "main.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern u32 tile_w;
extern u32 tile_h;

static const char* sprite_filenames[NUM_SPRITES] = 
{
#define SPRITE(name,a,b) #name,
    SPRITE_LIST
#undef SPRITE
};
static const u32 sprite_center[NUM_SPRITES*2] = 
{
#define SPRITE(name,center_x,center_y) center_x, center_y,
    SPRITE_LIST
#undef SPRITE
};
u32 sprite_size[NUM_SPRITES*2] = {};
static u8 *sprite_pixels[NUM_SPRITES] = {};
static SDL_Surface *sprite_surf[NUM_SPRITES] = {};
static SDL_Texture *sprite_texture[NUM_SPRITES] = {};

static const u32 rmask = 0x000000ff;
static const u32 gmask = 0x0000ff00;
static const u32 bmask = 0x00ff0000;
static const u32 amask = 0xff000000;


void load_sprites()
{
    char sprite_filename_buffer[128];
    for(int i=0;i<NUM_SPRITES;i++){
        sprintf(sprite_filename_buffer,"data/%s.png",sprite_filenames[i]);
        FILE *f = fopen(sprite_filename_buffer,"rb");
        if(f){
            s32 w,h,c;
            sprite_pixels[i] = stbi_load_from_file(f,&w,&h,&c,4);
            sprite_surf[i] = SDL_CreateRGBSurfaceFrom(sprite_pixels[i],w,h,32,
                    w*4,rmask,gmask,bmask,amask);
            sprite_texture[i] = SDL_CreateTextureFromSurface(renderer,
                    sprite_surf[i]);
            sprite_size[i*2] = w;
            sprite_size[i*2+1] = h;
            fclose(f);
        }
    }
}

void draw_sprite(u32 s, float x, float y, Camera camera)
{
    SDL_Rect dest_rect = {(x-sprite_center[s*2]-(sprite_size[s*2]-tile_w)*.5f)*camera.scale+camera.offset_x
        ,(y-sprite_center[s*2+1]-sprite_size[s*2+1]+tile_h)*camera.scale+camera.offset_y
        ,sprite_size[s*2]*camera.scale ,sprite_size[s*2+1]*camera.scale};
    SDL_RenderCopy(renderer,sprite_texture[s],0,&dest_rect);
}

void draw_sprite_rect(u32 s, float x1, float y1, float x2, float y2)
{
    SDL_Rect dest_rect = {x1,y1,x2-x1,y2-y1};
    SDL_RenderCopy(renderer,sprite_texture[s],0,&dest_rect);
}

void draw_sprite_clipped(u32 s, float x, float y, Camera camera)
{
    SDL_Rect dest_rect = {x*camera.scale+camera.offset_x
        ,y*camera.scale+camera.offset_y
        ,tile_w*camera.scale ,tile_h*camera.scale};
    SDL_Rect src_rect = {0,0,tile_w,tile_h};
    SDL_RenderCopy(renderer,sprite_texture[s],&src_rect,&dest_rect);
}

void draw_sprite_at_tile_with_alpha(u32 s, float x, float y, Camera camera,
        float alpha)
{
    u8 a = alpha*255;
    SDL_SetTextureAlphaMod(sprite_texture[s],a);
    draw_sprite(s,x*tile_w,y*tile_h, camera);
    SDL_SetTextureAlphaMod(sprite_texture[s],255);
}

void draw_sprite_at_tile(u32 s, float x, float y, Camera camera)
{
    draw_sprite(s,x*tile_w,y*tile_h, camera);
}

void draw_dialog(float x, float y, float w, float h, float scale)
{
    float border_x = 0.1f;
    float border_y = 0.1f;
    float sw = sprite_size[SPRITE_dialog*2];
    float sh = sprite_size[SPRITE_dialog*2+1];
    float x1 = border_x*sw;
    float y1 = border_x*sw;
    {
        SDL_Rect dest_rect = {x,y ,x1*scale,y1*scale};
        SDL_Rect src_rect = {0,0,x1, y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
    {
        SDL_Rect dest_rect = {x,y+y1*scale ,x1*scale,(h-y1)*scale};
        SDL_Rect src_rect = {0,y1,x1, sh-2*y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
    {
        SDL_Rect dest_rect = {x,y+(h-y1)*scale ,x1*scale,y1*scale};
        SDL_Rect src_rect = {0,sh-y1,x1,y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }

    {
        SDL_Rect dest_rect = {x+(w-x1)*scale,y ,x1*scale,y1*scale};
        SDL_Rect src_rect = {sw-x1,0,x1, y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
    {
        SDL_Rect dest_rect = {x+(w-x1)*scale,y+y1*scale ,x1*scale,(h-2*y1)*scale+1.f};
        SDL_Rect src_rect = {sw-x1,y1,x1, sh-2*y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
    {
        SDL_Rect dest_rect = {x+(w-x1)*scale,y+(h-y1)*scale ,x1*scale,y1*scale};
        SDL_Rect src_rect = {sw-x1,sh-y1,x1, y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }



    {
        SDL_Rect dest_rect = {x+(x1*scale),y ,(w-2*x1)*scale+1.f,y1*scale};
        SDL_Rect src_rect = {x1,0 ,sw*(1.f-2*border_x) ,y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
    {
        SDL_Rect dest_rect = {x+x1*scale,y+y1*scale ,(w-2*x1)*scale+1.f,(h-2*y1+1.f)*scale};
        SDL_Rect src_rect = {x1,y1 ,sw*(1.f-2*border_x) ,sh-2*y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
    {
        SDL_Rect dest_rect = {x+x1*scale,y+(h-y1)*scale ,(w-2*x1)*scale+1.f,y1*scale};
        SDL_Rect src_rect = {x1,sh-y1,sw*(1.f-2*border_x) ,y1};
        SDL_RenderCopy(renderer,sprite_texture[SPRITE_dialog],&src_rect,&dest_rect);
    }
}

