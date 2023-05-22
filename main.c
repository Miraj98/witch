#include <stdio.h>
#include "libavcodec/codec.h"
#include "libavcodec/codec_par.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_image.h>
#include <stdbool.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 800

int get_codec(const char* input, enum AVMediaType codec_type, int* stream_idx,
              AVFormatContext** fc,
              AVCodecContext** codec_ctx);
int setup_sdl(SDL_Window** window, SDL_Renderer** renderer);
void quit_sdl();

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Provide a video file.\n");
        return -1;
    }

    printf("File path provided: %s\n", argv[1]);

    /* Setting up SDL*/
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    if (setup_sdl(&window, &renderer) != 0) {
        return -1;
    };

    /* Setting video codec */
    AVFormatContext* format_context = NULL;
    AVCodecContext* video_codec_ctx = NULL;
    int video_stream_id = -1;
    int ret = get_codec(argv[1], AVMEDIA_TYPE_VIDEO,
                        &video_stream_id, &format_context, &video_codec_ctx);
    if (ret < 0) { return ret; }
    AVPacket pkt;
    AVFrame* frame = av_frame_alloc();

    SDL_Rect destrect = {.x = 0, .y = 0, .h = -1, .w = -1};

    /* Render loop */
    int quit_render = 0;
    SDL_Event e;
    while (!quit_render) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT){
                quit_render = true;
            }
            if (e.type == SDL_KEYDOWN){
                quit_render = true;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN){
                quit_render = true;
            }
        }

        if (av_read_frame(format_context, &pkt) < 0) {
            quit_render = 1;
        }

        if (pkt.stream_index != video_stream_id) {
            continue;
        }

        if (avcodec_send_packet(video_codec_ctx, &pkt) < 0) {
            quit_render = 1;
            fprintf(stderr, "Error sending packet to the codec\n");
        }

        while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
            if (destrect.h == -1) {
                destrect.h = frame->height;
                if (frame->width > WINDOW_WIDTH) {
                    destrect.w = WINDOW_WIDTH;
                    destrect.h = (frame->height * destrect.w)/frame->width;
                } else {
                    destrect.w = frame->width;
                }
            }
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
            if (!texture) {
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                quit_sdl();
                return -1;
            }
            SDL_UpdateYUVTexture(texture, NULL,
                                 frame->data[0], frame->linesize[0],
                                 frame->data[1], frame->linesize[1],
                                 frame->data[2], frame->linesize[2]);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, &destrect);
            SDL_RenderPresent(renderer);
            SDL_Delay(1000/30);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

int get_codec(
    const char* input,
    enum AVMediaType codec_type,
    int* stream_idx,
    AVFormatContext** format_ctx,
    AVCodecContext** _cod_ctx
) {
    if (*format_ctx == NULL) {
        if (avformat_open_input(format_ctx, input, NULL, NULL) != 0) {
            fprintf(stderr, "Error opening the input file.\n");
            return -1;
        }

        if (avformat_find_stream_info(*format_ctx, NULL) < 0) {
            fprintf(stderr, "Error finding stream info\n");
            return -2;
        }
    }

    AVCodecParameters* codec_params = NULL;
    const AVCodec* codec = NULL;

    for (int i = 0; i < (*format_ctx)->nb_streams; i++) {
        if ((*format_ctx)->streams[i]->codecpar->codec_type == codec_type) {
            *stream_idx = i;
            codec_params = (*format_ctx)->streams[i]->codecpar;
            codec = avcodec_find_decoder(codec_params->codec_id);
            break;
        }
    }

    if (*stream_idx == -1 || codec_params == NULL || codec == NULL) {
        fprintf(stderr, "Error finding video stream or decoder");
        return -3;
    }

    if (*_cod_ctx == NULL) {
        *_cod_ctx = avcodec_alloc_context3(codec);
        if (_cod_ctx == NULL) {
            fprintf(stderr, "Failed to allocate a codec\n");
            return -4;
        }
    }

    if (avcodec_parameters_to_context(*_cod_ctx, codec_params) < 0) {
        fprintf(stderr, "Failed to copy codec params to context\n");
        return -5;
    }

    if (avcodec_open2(*_cod_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Failed to open the codec\n");
        return -6;
    }

    return 0;
}

int setup_sdl(SDL_Window** window, SDL_Renderer** rend) {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[SDL ERROR] %s\n", SDL_GetError());
        return -1;
    }

    *window = SDL_CreateWindow("Video streamer",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!(*window)) {
        quit_sdl();
        return -1;
    }

    Uint32 render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    *rend = SDL_CreateRenderer(*window, -1, render_flags);


    if (!(*rend)) {
        SDL_DestroyWindow(*window);
        quit_sdl();
        return -1;
    }

    return 0;
}

void quit_sdl() {
    fprintf(stderr, "[SDL ERROR] %s\n", SDL_GetError());
    SDL_Quit();
}
