gcc main.c\
	-lavcodec -lavformat -lswresample\
	-lSDL2 -lSDL2_image\
	-o build/main\
	-I include\
	-I /opt/homebrew/Cellar/ffmpeg/6.0/include\
	-I /opt/homebrew/Cellar/sdl2/2.26.5/include/SDL2\
	-I /opt/homebrew/Cellar/sdl2_image/2.6.3_1/include/SDL2\
	-L lib\
	
