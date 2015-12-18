#include "sound.h"
#ifdef __EMSCRIPTEN__
#include <SDL/SDL_mixer.h>
#else
#include <SDL2/SDL_mixer.h>
#endif


#ifdef DEBUG
    static const char* sound_filenames[NUM_SOUNDS] = 
    {
    #define SOUND(name) "data/" #name ".wav",
        SOUND_LIST
    #undef SOUND
    };
#else
    #include "incbin.h"
    #define SOUND(name) INCBIN(name, "data/" #name ".wav");
        SOUND_LIST
    #undef SOUND
    const u8 *sound_data[] = {
    #define SOUND(name) g ## name ## Data,
        SOUND_LIST
    #undef SOUND
    };
    u32 sound_filesize[NUM_SOUNDS];
    static void init_sound_filesizes(){
        u32 i=0;
    #define SOUND(name) sound_filesize[i++] =  g ## name ## Size;
        SOUND_LIST
    #undef SOUND
    }

    INCBIN(music,"data/music.ogg");
#endif


static Mix_Chunk *sound_cunks[NUM_SOUNDS] = {};
static Mix_Music *music = 0;

void sound_init()
{
    Mix_Init(MIX_INIT_OGG);
    Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024);

#ifdef DEBUG
    for(int i = 0; i< NUM_SOUNDS; i++){
        sound_cunks[i] = Mix_LoadWAV(sound_filenames[i]); 
    }
    SDL_RWops* rw = SDL_RWFromFile("data/music.ogg","rb");
    u8 free_me = 1;
#else
    init_sound_filesizes();
    for(int i = 0; i< NUM_SOUNDS; i++){
        SDL_RWops* rw = SDL_RWFromMem((void*)sound_data[i],sound_filesize[i]);
        sound_cunks[i] = Mix_LoadWAV_RW(rw,0); 
    }
    SDL_RWops* rw = SDL_RWFromMem((void*)gmusicData,gmusicSize);
    u8 free_me = 0;
#endif
    music = Mix_LoadMUS_RW(rw,free_me);
}

void sound_quit()
{
    Mix_CloseAudio();
    while(Mix_Init(0))
        Mix_Quit();
    for(int i = 0; i< NUM_SOUNDS; i++){
        Mix_FreeChunk(sound_cunks[i]); 
    }
}

void play_sound(u32 sound, float volume)
{
    assert(sound < NUM_SOUNDS);
    s32 c = Mix_PlayChannel( -1, sound_cunks[sound], 0 );
    Mix_Volume(c,(u8)(volume*128));
}

void play_music()
{
    Mix_PlayMusic( music, -1 );
}
