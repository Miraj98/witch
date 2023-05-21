gcc -o build/main\
	main.c\
	-I/opt/homebrew/Cellar/ffmpeg/6.0/include -lavcodec -lavformat\
	-I/opt/homebrew/Cellar/sdl2/2.26.5/include/SDL2 -lSDL2\
	-I/opt/homebrew/Cellar/sdl2_image/2.6.3_1/include/SDL2 -lSDL2_image\
	
