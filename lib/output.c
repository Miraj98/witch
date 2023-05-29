#include <SDL.h>
#include <libavcodec/avcodec.h>

#ifndef OUTPUT_H
#define OUTPUT_H
#include "typedefs.c"

#define PRINT_SDL_ERROR() fprintf(stderr, "[SDL ERROR] %s\n", SDL_GetError())
#define RENDER_FLAGS (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
#define WINDOW_HEIGHT 800
#define WINDOW_WIDTH 640

int setup_sdl(MediaPlayerState *m) {
    m->display->window = SDL_CreateWindow("Video streamer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!m->display->window) {
        PRINT_SDL_ERROR();
        SDL_Quit();
        return -1;
    }

    m->display->renderer = SDL_CreateRenderer(m->display->window, -1, RENDER_FLAGS);
    m->display->texture = SDL_CreateTexture(m->display->renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m->video_codec_ctx->width, m->video_codec_ctx->height);
    const char* error = SDL_GetError();
    if (!m->display->renderer) {
        PRINT_SDL_ERROR();
        SDL_DestroyWindow(m->display->window);
        return -1;
    }

    if (m->audio_device_id != 0) {
        SDL_AudioSpec obtained;
        SDL_AudioSpec desired = { .freq =  m->audio_codec_ctx->sample_rate, .format = AUDIO_S16SYS,
                                .channels = m->audio_codec_ctx->ch_layout.nb_channels, .callback = NULL,
                                .silence = 0, .samples = m->audio_codec_ctx->frame_size, .userdata = m };
        m->audio_device_id = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (m->audio_device_id == 0) {
            PRINT_SDL_ERROR();
            return -1;
        }
    }

    return 0;
}

int display_frame(MediaPlayerState *m) {
    printf("display frame is being called.\n");
    AVFrame frame;
    SDL_LockMutex(m->framebuffer_mutex);
    while (1) {
        if (m->framebuffer[m->frame_read_index].allocated == 1) {
            frame = m->framebuffer[m->frame_read_index].frame;
            SDL_UpdateYUVTexture(m->display->texture, NULL, frame.data[0], frame.linesize[0],
                                frame.data[1], frame.linesize[1], frame.data[2],
                                frame.linesize[2]);
            const char *error = SDL_GetError();
            SDL_RenderCopy(m->display->renderer, m->display->texture, NULL, &m->display->rect);
            SDL_RenderPresent(m->display->renderer);
            m->framebuffer[m->frame_read_index].allocated = 0;
            // av_frame_free(m->framebuffer[m->frame_read_index].frame);
            m->frame_read_index = (m->frame_read_index + 1) % VIDEO_FRAME_BUFFER_SIZE;
            SDL_CondSignal(m->framebuffer_cond);
            break;
        } else {
            SDL_CondWait(m->framebuffer_cond, m->framebuffer_mutex);
        }
    }
    SDL_UnlockMutex(m->framebuffer_mutex);

    return 16;
}

int audio_callback() {}

#endif