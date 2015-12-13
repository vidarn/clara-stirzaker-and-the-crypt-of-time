#include "sound.h"
#ifdef __EMSCRIPTEN__
#include <SDL/SDL_mixer.h>
#else
#include <SDL2/SDL_mixer.h>
#endif

static const char* sound_filenames[NUM_SOUNDS] = 
{
#define SOUND(name) "data/" #name ".wav",
    SOUND_LIST
#undef SOUND
};
static Mix_Chunk *sound_cunks[NUM_SOUNDS] = {};
static Mix_Music *music = 0;

void sound_init()
{
    Mix_Init(MIX_INIT_OGG);
    Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024);
    for(int i = 0; i< NUM_SOUNDS; i++){
        sound_cunks[i] = Mix_LoadWAV(sound_filenames[i]); 
    }
    music = Mix_LoadMUS( "data/music.ogg" );
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
    Mix_Volume(c,volume*128);
}

void play_music()
{
    Mix_PlayMusic( music, -1 );
}
