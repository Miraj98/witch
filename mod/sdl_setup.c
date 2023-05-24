#include <SDL.h>
#include <libavcodec/avcodec.h>

#define PRINT_SDL_ERROR() fprintf(stderr, "[SDL ERROR] %s\n", SDL_GetError())

int setup_renderer(SDL_Window **window,
                    const char *wTitle,
                    SDL_Renderer **renderer,
                    int wHeight, int wWidth)
{
    *window = SDL_CreateWindow(wTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, wWidth, wHeight, 0);
    if (!(*window)) {
        PRINT_SDL_ERROR();
        SDL_Quit();
        return -1;
    }

    Uint32 render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    *renderer = SDL_CreateRenderer(*window, -1, render_flags);
    if (!(*renderer)) {
        PRINT_SDL_ERROR();
        SDL_DestroyWindow(*window);
        return -1;
    }

    return 0;
}

int setup_audio(const char *devicename, AVCodecContext *ctx, SDL_AudioCallback callback) {
    SDL_AudioSpec obtained;
    SDL_AudioSpec desired = { .freq =  ctx->sample_rate, .format = AUDIO_S16SYS,
                              .channels = ctx->ch_layout.nb_channels, .callback = callback,
                              .silence = 0, .samples = ctx->frame_size, .userdata = ctx };
    int device_id = SDL_OpenAudioDevice(devicename, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (device_id == 0) {
        PRINT_SDL_ERROR();
        return -1;
    }

    return device_id;
}