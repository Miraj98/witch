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
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

/* User defined structs */
typedef struct PacketNode {
    AVPacket pkt;
    struct PacketNode *next;
} PacketNode;

typedef struct {
    PacketNode *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

/* Global variables */
int quit = 0;
PacketQueue queue;

/* Function declarations */
int get_codec(const char* input, enum AVMediaType codec_type, int* stream_idx,
              AVFormatContext** fc,
              AVCodecContext** codec_ctx);
int setup_sdl(SDL_Window** window, SDL_Renderer** renderer);
void quit_sdl();
void audio_callback(void *userdata, Uint8 *stream, int len);


/* Function definations */
void packet_queue_init(PacketQueue* q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt) {
    PacketNode* pktl;

    pktl = av_malloc(sizeof(PacketNode));
    if (!pktl) {
        return -1;
    }

    pktl->pkt = *pkt;
    pktl->next = NULL;

    SDL_LockMutex(q->mutex);
    if (!q->last_pkt) {
        q->first_pkt = pktl;
    } else {
        q->last_pkt->next = pktl;
    }
    q->last_pkt = pktl;
    q->nb_packets++;
    q->size += pkt->size;

    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    PacketNode* pkt_node;
    int ret = 0;

    SDL_LockMutex(q->mutex);

    for(;;) {
        if (quit) {
            ret = -1;
            break;
        }

        pkt_node = q->first_pkt;
        if (pkt_node) {
            q->first_pkt = pkt_node->next;
            if (!q->first_pkt) q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt_node->pkt.size;
            *pkt = pkt_node->pkt;
            av_free(pkt_node);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

/* Entry point */
int main(int argc, char* argv[]) {
    // if (argc < 2) {
    //     fprintf(stderr, "Provide a video file.\n");
    //     return -1;
    // }

    // char *input = argv[1];
    char *input = "av.mp4";



    /* Setting up SDL*/
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    if (setup_sdl(&window, &renderer) != 0) {
        return -1;
    };

    /* Setting video codec */
    AVFormatContext* format_context = NULL;
    AVCodecContext* video_codec_ctx = NULL;
    AVCodecContext* audio_codec_ctx = NULL;

    int video_stream_id = -1;
    int audio_stream_id = -1;
    
    SDL_AudioSpec wanted_spec, spec;

    int ret = get_codec(input, AVMEDIA_TYPE_VIDEO,
                        &video_stream_id, &format_context, &video_codec_ctx);
    if (ret < 0) { return ret; }

    get_codec(input, AVMEDIA_TYPE_AUDIO, &audio_stream_id, &format_context, &audio_codec_ctx);

    if (audio_stream_id != -1) {
        packet_queue_init(&queue);
        wanted_spec.freq = audio_codec_ctx->sample_rate;
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.channels = audio_codec_ctx->channels;
        wanted_spec.silence = 0;
        wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
        wanted_spec.callback = audio_callback;
        wanted_spec.userdata = audio_codec_ctx;

        if (SDL_OpenAudio(&wanted_spec, &spec) != 0) {
            fprintf("[SDL ERROR] %c\n", SDL_GetError());
            return -1;
        }

        SDL_PauseAudio(0);
    }

    AVPacket pkt;
    AVFrame* frame = av_frame_alloc();

    SDL_Rect destrect = {.x = 0, .y = 0, .h = -1, .w = -1};

    /* Render loop */
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                case SDL_KEYDOWN:
                case SDL_MOUSEBUTTONDOWN:
                    quit = true;
                    break;
            }
        }

        if (av_read_frame(format_context, &pkt) < 0) {
            quit = 1;
        }

        if (pkt.stream_index == video_stream_id) {
            if (avcodec_send_packet(video_codec_ctx, &pkt) < 0) {
                quit = 1;
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
        } else if (pkt.stream_index == audio_stream_id) {
            packet_queue_put(&queue, &pkt);
        } else {
            av_packet_free(&pkt);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

/* Function definations */
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

int audio_decode_frame(AVCodecContext* audio_codec_ctx, uint8_t* audio_buf, int buf_size) {
    AVPacket pkt;
    AVFrame audio_frame;
    if (packet_queue_get(&queue, &pkt, 1) == 1) {
        if (avcodec_send_packet(audio_codec_ctx, &pkt) != 0 ) {
            fprintf(stderr, "[FFMPEG ERROR] Unable to send packet to the decoder. Have you already opened the decoder using avcodec_open2?\n.");
            return -1;
        }

        int num_frames = 0;

        while (avcodec_receive_frame(audio_codec_ctx, &audio_frame) == 0) {
            num_frames++;
            // int bufsize = av_samples_get_buffer_size(NULL, audio_codec_ctx->channels, audio_frame.nb_samples, audio_codec_ctx->sample_fmt, 1);
            // memcpy(audio_buf, audio_frame.data[0], bufsize);
        }

        printf("Number of audio frames: %d\n", num_frames);
    }

    return -1;
}

void audio_callback(void* userdata, Uint8* stream, int len) {
    AVCodecContext* audio_codec_context = (AVCodecContext *) userdata;
    int len1, audio_size;
    static uint8_t audio_buf[(3 * MAX_AUDIO_FRAME_SIZE) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    /*
        Basically copy the decoded audio data to the audio buffer (Uint8 *stream) of the device.
        Important to handle the case where if the amount of buffer read from the decoder is less than the 
        buffer required to fill.
    */
    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            audio_size = audio_decode_frame(audio_codec_context, audio_buf, sizeof(audio_buf));
            if (audio_size < 0) {
                audio_buf_size = 1024;
                memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }
        memcpy(stream, (uint8_t *) audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}
