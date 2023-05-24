#include "libavcodec/codec.h"
#include "libavcodec/codec_par.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_timer.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdbool.h>
#include <stdio.h>
#include "mod/decoder.c"
#include "mod/sdl_setup.c"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 800
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define SDL_INIT_FLAGS SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_VIDEO
#define IS_VIDEO_INITED SDL_INIT_VIDEO & (SDL_INIT_FLAGS)
#define IS_AUDIO_INITED SDL_INIT_AUDIO & (SDL_INIT_FLAGS)


#if IS_AUDIO_INITED
uint8_t *audio_buffer = NULL;
int audio_buffer_size = 0;
int audio_buffer_index = 0;
#endif

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

void audio_callback(void *userdata, Uint8 *stream, int len);

/* Function definations */
void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
  PacketNode *pktl;

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
  PacketNode *pkt_node;
  int ret = 0;

  SDL_LockMutex(q->mutex);

  for (;;) {
    if (quit) {
      ret = -1;
      break;
    }

    pkt_node = q->first_pkt;
    if (pkt_node) {
      q->first_pkt = pkt_node->next;
      if (!q->first_pkt)
        q->last_pkt = NULL;
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
int main(int argc, char *argv[]) {
  // if (argc < 2) {
  //     fprintf(stderr, "Provide a video file.\n");
  //     return -1;
  // }

  // char *input = argv[1];
  char *input = "av2.mp4";

  audio_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);

  /* Setting up SDL*/
  SDL_Init(SDL_INIT_FLAGS);

  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  if (SDL_INIT_FLAGS & SDL_INIT_VIDEO) {
    if (setup_renderer(&window, "Test", &renderer, WINDOW_HEIGHT, WINDOW_WIDTH) != 0) {
        return -1;
    }
  }
  

  /* Setting video codec */
  AVFormatContext *format_context = NULL;
  AVCodecContext *video_codec_ctx = NULL;
  int video_stream_id = -1;
  AVCodecContext *audio_codec_ctx = NULL;
  int audio_stream_id = -1;
  int deviceId = -1;

  #if IS_VIDEO_INITED
  video_stream_id = open_codec(input, AVMEDIA_TYPE_VIDEO,
                      &format_context, &video_codec_ctx);
  if (video_stream_id < 0) {
    return -1;
  }

  SDL_Rect destrect = {.x = 0, .y = 0, .h = -1, .w = -1};
  #endif

  #if IS_AUDIO_INITED
  audio_stream_id = open_codec(input, AVMEDIA_TYPE_AUDIO, &format_context,
            &audio_codec_ctx);
  deviceId = setup_audio(NULL, audio_codec_ctx, audio_callback);
  SDL_PauseAudioDevice(deviceId, 0);
  #endif

  AVPacket pkt;
  AVFrame *frame = av_frame_alloc();

    AVCodecParameters *codecParams = format_context->streams[audio_stream_id]->codecpar;
    int64_t duration = format_context->duration;  // duration in AV_TIME_BASE units
    int64_t sampleCount = (int64_t)codecParams->sample_rate * duration / AV_TIME_BASE;


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

    #if IS_VIDEO_INITED
    if (pkt.stream_index == video_stream_id) {
      if (avcodec_send_packet(video_codec_ctx, &pkt) < 0) {
        quit = 1;
        fprintf(stderr, "Error sending packet to the codec\n");
      }

      while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
        SDL_RenderClear(renderer);
        if (destrect.h == -1) {
          destrect.h = frame->height;
          if (frame->width > WINDOW_WIDTH) {
            destrect.w = WINDOW_WIDTH;
            destrect.h = (frame->height * destrect.w) / frame->width;
          } else {
            destrect.w = frame->width;
          }
        }
        SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 frame->width, frame->height);
        if (!texture) {
          SDL_DestroyRenderer(renderer);
          SDL_DestroyWindow(window);
          PRINT_SDL_ERROR();
          SDL_Quit();
          return -1;
        }
        SDL_UpdateYUVTexture(texture, NULL, frame->data[0], frame->linesize[0],
                             frame->data[1], frame->linesize[1], frame->data[2],
                             frame->linesize[2]);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &destrect);
        SDL_RenderPresent(renderer);
        // SDL_Delay(1000/30);
      }
    }
    #endif

    #if IS_AUDIO_INITED
    if (pkt.stream_index == audio_stream_id) {
      packet_queue_put(&queue, &pkt);
      // if (avcodec_send_packet(audio_codec_ctx, &pkt) < 0) {
      //     quit = 1;
      //     fprintf(stderr, "Error sending packet to the codec\n");
      // }

      // if (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
      //     int data_size = av_samples_get_buffer_size(NULL, audio_codec_ctx->ch_layout.nb_channels, frame->nb_samples, audio_codec_ctx->sample_fmt, 1);
      //     int queue_audio = SDL_QueueAudio(deviceId, frame->data[0], data_size);
      //     if (queue_audio != 0) {
      //       PRINT_SDL_ERROR();
      //     }
      // }
    }
    #endif
  }

  #if IS_VIDEO_INITED
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  #endif
  SDL_Quit();

  return 0;
}

/* Function definations */

int audio_decode_frame(AVCodecContext *audio_codec_ctx, uint8_t *audio_buf,
                       int buf_size) {
  AVPacket pkt;
  AVFrame *audio_frame = av_frame_alloc();
  int data_size = 0;

  while (packet_queue_get(&queue, &pkt, 1) == 1) {
    if (avcodec_send_packet(audio_codec_ctx, &pkt) != 0) {
      fprintf(stderr,
              "[FFMPEG ERROR] Unable to send packet to the decoder. Have you "
              "already opened the decoder using avcodec_open2?\n.");
      return -1;
    }

    while (avcodec_receive_frame(audio_codec_ctx, audio_frame) == 0) {
      int plane_size = av_get_bytes_per_sample(audio_codec_ctx->sample_fmt);
      int bufsize = plane_size * audio_frame->nb_samples * audio_frame->ch_layout.nb_channels;
      if (data_size + bufsize > buf_size) {
        fprintf(stderr, "Audio buffer overflow\n");
        break;
      }
      for (int i = 0; i < audio_frame->nb_samples; i++) {
        for (int j = 0; j < audio_frame->ch_layout.nb_channels; j++) {
          memcpy(audio_buf + data_size, audio_frame->data[j] + i * plane_size, plane_size);
          data_size += plane_size;
        }
      }
    }

    av_packet_unref(&pkt);
  }

  av_frame_free(&audio_frame);

  return data_size;
}


// int audio_decode_frame(AVCodecContext *audio_codec_ctx, uint8_t *audio_buf,
//                        int buf_size) {
//   AVPacket pkt;
//   AVFrame *audio_frame = av_frame_alloc();
//   if (packet_queue_get(&queue, &pkt, 1) == 1) {
//     if (avcodec_send_packet(audio_codec_ctx, &pkt) != 0) {
//       fprintf(stderr,
//               "[FFMPEG ERROR] Unable to send packet to the decoder. Have you "
//               "already opened the decoder using avcodec_open2?\n.");
//       return -1;
//     }

//     int num_frames = 0;
//     int data_size = 0;

//     while (avcodec_receive_frame(audio_codec_ctx, audio_frame) == 0) {
//       num_frames++;
//       int bufsize = av_samples_get_buffer_size(NULL, audio_frame->ch_layout.nb_channels,
//                                                audio_frame->nb_samples,
//                                                audio_codec_ctx->sample_fmt, 1);
//       SDL_memcpy4(audio_buf, audio_frame->data[0], bufsize);
//       data_size += bufsize;
//     }

//     SDL_assert(data_size <= buf_size);

//     return data_size;

//     printf("Number of audio frames: %d\n", num_frames);
//   }

//   av_frame_free(&audio_frame);

//   return -1;
// }


void audio_callback(void *userdata, Uint8 *stream, int len) {
  SDL_memset(stream, 0, len);
  AVCodecContext *audio_codec_context = (AVCodecContext *)userdata;

  if (audio_buffer_index >= audio_buffer_size) {
    audio_buffer_index = 0;
    audio_buffer_size = audio_decode_frame(audio_codec_context, audio_buffer, MAX_AUDIO_FRAME_SIZE);
    if (audio_buffer_size < 0) {
      fprintf(stderr, "Error decoding frame");
      SDL_memset(stream, 0, len);
      return;
    }
  }
  int buffer_to_copy = audio_buffer_size - audio_buffer_index;
  if (buffer_to_copy > len) {
    buffer_to_copy = len;
  }

  SDL_memcpy4(stream, audio_buffer + audio_buffer_index, buffer_to_copy);

  audio_buffer_index += buffer_to_copy;
}
