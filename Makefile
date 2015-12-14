default:debug

all:native web

sources = main.c editor.c menu.c easing.c game.c sprite.c assets.c sound.c
output = clara_stirzaker_and_the_crypt_of_time


CC = gcc -std=gnu11
#CC = clang

optimization  = -O0
#optimization  = -O1 -fsanitize=address -fno-omit-frame-pointer
#optimization  = -O1 -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer
#optimization  = -O3

web:
	source emsdk_portable/emsdk_set_env.sh;\
	emcc -O3 $(sources) -o $(output).html -s USE_SDL=2 -s USE_SDL_TTF=2 \
	-s ALLOW_MEMORY_GROWTH=1 --preload-file data/
	cp $(output).html $(output).data $(output).js $(output).html.mem site
	@rclone copy site dropbox:Public/ld34

web-devel:
	source emsdk_portable/emsdk_set_env.sh;\
	emcc $(sources) -o $(output).html -s USE_SDL=2 -s USE_SDL_TTF=2 \
	-s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=2 --preload-file data/

native:
	$(CC) $(sources) -g -O3 -o $(output) -lSDL2 -lSDL2_ttf -lSDL2_mixer -lm \

mingw:
	$(CC) $(sources) -g -O3 -o $(output) -lSDL2 -lSDL2_ttf -lSDL2_mixer -lm \
		-Wl,-subsystem,windows

debug:
	$(CC) $(sources) -g -O0 -D DEBUG -o $(output) -lSDL2 -lSDL2_ttf -lSDL2_mixer -lm
	
windows:
	i686-w64-mingw32-clang $(sources) -g -O0 -o $(output).exe -lSDL2 -lSDL2_mixer -lSDL2_ttf -lm -Wl,-subsystem,windows
	zip -r $(output).zip *.exe *.dll
	cp $(output).zip site
	@rclone copy site dropbox:Public/ld34

run:debug
	optirun ./$(output);echo a
