#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
// #include <SDL2/SDL_image.h>

int main() {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[SDL] %s", SDL_GetError());
        return -1;
    }

    printf("SDL inited correctly I guess\n");

    SDL_Window* window = SDL_CreateWindow("Video streamer",
                                            SDL_WINDOWPOS_CENTERED,
                                            SDL_WINDOWPOS_CENTERED,
                                            640, 800, 0);
    if (window == NULL) {
        fprintf(stderr, "[SDL] %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Delay(5000);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;

}