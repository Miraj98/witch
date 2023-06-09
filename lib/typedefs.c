#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <SDL.h>

#ifndef TYPEDEFS
#define TYPEDEFS

#define VIDEO_FRAME_BUFFER_SIZE 10
#define REFRESH_VIDEO_DISPLAY (SDL_USEREVENT + 1)

typedef struct FrameBufferItem {
    AVFrame frame;
    int allocated;
} FrameBufferItem;

typedef struct PacketItem {
    AVPacket pkt;
    struct PacketItem *next;
} PacketItem;

typedef struct PacketQueue {
    PacketItem *first, *last;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

typedef struct DisplayOutput {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Rect rect;
} DisplayOutput;

typedef struct MediaPlayerState {
    AVFormatContext *fmt_ctx;
    AVCodecContext *video_codec_ctx, *audio_codec_ctx;
    SwrContext *resampler_ctx;
    int video_stream_id, audio_stream_id;
    int audio_device_id;
    DisplayOutput *display;

    FrameBufferItem framebuffer[VIDEO_FRAME_BUFFER_SIZE];
    int frame_read_index;
    int frame_write_index;
    SDL_mutex *framebuffer_mutex;
    SDL_cond *framebuffer_cond;
    int audio_buffer_index;
    int audio_buffer_size;
    uint8_t *audio_buffer;

    PacketQueue video_pkt_queue, audio_pkt_queue;

    SDL_Thread *decoder_tid, *video_tid;

    int quit;
} MediaPlayerState;

MediaPlayerState* alloc_media_player_state() {
    MediaPlayerState *m = (MediaPlayerState *)av_mallocz(sizeof(MediaPlayerState));
    if (!m) {
        fprintf(stderr, "Failed to allocate memory for the media player.\n");
        return NULL;
    }
    m->video_stream_id = -1;
    m->audio_stream_id = -1;

    m->framebuffer_mutex = SDL_CreateMutex();
    m->framebuffer_cond = SDL_CreateCond();

    m->video_pkt_queue.mutex = SDL_CreateMutex();
    m->video_pkt_queue.cond = SDL_CreateCond();

    m->audio_pkt_queue.mutex = SDL_CreateMutex();
    m->audio_pkt_queue.cond = SDL_CreateCond();

    m->display = SDL_malloc(sizeof(DisplayOutput));
    m->display->rect.h = -1;
    m->display->rect.w = -1;
    m->display->rect.x = 0;
    m->display->rect.y = 0;

    return m;
}

int pkt_queue_put(PacketQueue *pkt_queue, AVPacket *pkt) {
    PacketItem *pkt_item = av_malloc(sizeof(PacketItem));
    if (!pkt_item) {
        fprintf(stderr, "Failed to allocate a PacketItem for the pkt_queue_put op.\n");
        return -1;
    }
    pkt_item->pkt = *pkt;
    pkt_item->next = NULL;

    SDL_LockMutex(pkt_queue->mutex);
    if (pkt_queue->last == NULL) {
        pkt_queue->first = pkt_item;
    } else {
        pkt_queue->last->next = pkt_item;
    }
    pkt_queue->last = pkt_item;
    pkt_queue->nb_packets++;
    pkt_queue->size += pkt->size;
    SDL_CondSignal(pkt_queue->cond);
    SDL_UnlockMutex(pkt_queue->mutex);

    return 0;
};

int pkt_queue_get(PacketQueue *pkt_queue, AVPacket *pkt, MediaPlayerState *m) {
    SDL_LockMutex(pkt_queue->mutex);
    PacketItem *pkt_item = NULL;
    int ret = 0;

    while (1) {
        if (m->quit) {
            ret = -1;
        }

        if (pkt_queue->first) {
            pkt_item = pkt_queue->first;
            pkt_queue->first = pkt_item->next;
            if (pkt_queue->first == NULL) {
                pkt_queue->last = NULL;
            }
            pkt_queue->nb_packets--;
            pkt_queue->size -= pkt_item->pkt.size;
            *pkt = pkt_item->pkt;
            av_free(pkt_item);
            break;
        } else {
            SDL_CondWait(pkt_queue->cond, pkt_queue->mutex);
        }
    }
    SDL_UnlockMutex(pkt_queue->mutex);
    return ret;
}

#endif