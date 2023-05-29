#include <SDL.h>
#include <libavcodec/avcodec.h>

#ifndef OUTPUT_H
#define OUTPUT_H
#include "typedefs.c"

#define PRINT_SDL_ERROR() fprintf(stderr, "[SDL ERROR] %s\n", SDL_GetError())
#define RENDER_FLAGS (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
#define WINDOW_HEIGHT 800
#define WINDOW_WIDTH 640
#define MAX_AUDIO_FRAME_SIZE 192000
#define SDL_INIT_FLAGS (SDL_INIT_VIDEO | SDL_INIT_AUDIO)

void audio_callback(void *userdata, Uint8 *stream, int len);

int setup_sdl(MediaPlayerState *m) {
    SDL_Init(SDL_INIT_FLAGS);
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

    if (m->audio_stream_id > 0) {
        SDL_AudioSpec obtained;
        SDL_AudioSpec desired = { .freq =  m->audio_codec_ctx->sample_rate, .format = AUDIO_S16SYS,
                                .channels = m->audio_codec_ctx->ch_layout.nb_channels, .callback = audio_callback,
                                .silence = 0, .samples = m->audio_codec_ctx->frame_size, .userdata = m };
        m->audio_device_id = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (m->audio_device_id == 0) {
            PRINT_SDL_ERROR();
            return -1;
        }
        SDL_PauseAudioDevice(m->audio_device_id, 0);
    }

    return 0;
}

int display_frame(MediaPlayerState *m) {
    AVFrame frame;
    SDL_LockMutex(m->framebuffer_mutex);
    while (1) {
        if (m->framebuffer[m->frame_read_index].allocated == 1) {
            frame = m->framebuffer[m->frame_read_index].frame;
            SDL_RenderClear(m->display->renderer);
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

    SDL_Delay(1000/30);

    return 16;
}

int audio_decode_frame(MediaPlayerState *m) {
  AVPacket pkt;
  AVFrame *audio_frame = av_frame_alloc();
  if (pkt_queue_get(&m->audio_pkt_queue, &pkt, m) == 1) {
    if (avcodec_send_packet(m->audio_codec_ctx, &pkt) != 0) {
      fprintf(stderr,
              "[FFMPEG ERROR] Unable to send packet to the decoder. Have you "
              "already opened the decoder using avcodec_open2?\n.");
      return -1;
    }

    int num_frames = 0;
    int data_size = 0;

    while (avcodec_receive_frame(m->audio_codec_ctx, audio_frame) == 0) {
      num_frames++;
      uint8_t *out[] = {m->audio_buffer};
      int out_samples = swr_convert(m->resampler_ctx, out, audio_frame->nb_samples, (const uint8_t **) audio_frame->data, audio_frame->nb_samples);
      int bufsize = out_samples * m-> audio_codec_ctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
      SDL_memcpy4(m->audio_buffer, audio_frame->data[0], bufsize);
      data_size += bufsize;
    }

    SDL_assert(data_size <= MAX_AUDIO_FRAME_SIZE);

    return data_size;

    printf("Number of audio frames: %d\n", num_frames);
  }

  av_frame_free(&audio_frame);

  return -1;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    printf("Audio callback being called.\n");
    SDL_memset4(stream, 0, len);
    MediaPlayerState *m = (MediaPlayerState *)userdata;

    if (m->audio_buffer_index >= m->audio_buffer_size) {
        m->audio_buffer_index = 0;
        m->audio_buffer_size = audio_decode_frame(m);
        if (m->audio_buffer_size < 0) {
            fprintf(stderr, "Error decoding frame");
            SDL_memset(stream, 0, len);
            return;
        }
    }
    int buffer_to_copy = m->audio_buffer_size - m->audio_buffer_index;
    if (buffer_to_copy > len) {
        buffer_to_copy = len;
    }

    SDL_memcpy4(stream, m->audio_buffer + m->audio_buffer_index, buffer_to_copy);
    m->audio_buffer_index += buffer_to_copy;
}

#endif