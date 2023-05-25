#include "typedefs.c"
#include <libswresample/swresample.h>

int video_decoder(void *);

int open_codec(const char *filepath, MediaPlayerState *mp)
{
    if (mp->fmt_ctx == NULL) {
        if (avformat_open_input(&mp->fmt_ctx, filepath, NULL, NULL) != 0) {
            fprintf(stderr, "Error opening the input.\n");
            return -1;
        }

        if (avformat_find_stream_info(mp->fmt_ctx, NULL) < 0) {
            fprintf(stderr, "Error finding stream info.\n");
            return -1;
        }
    }

    for (int i = 0; i < (mp->fmt_ctx)->nb_streams; i++) {
        switch (mp->fmt_ctx->streams[i]->codecpar->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                mp->audio_stream_id = i;
                const AVCodec *aCodec = avcodec_find_decoder(mp->fmt_ctx->streams[i]->codecpar->codec_id);
                if (aCodec == NULL) {
                    fprintf(stderr, "Unable to find the decoder.\n");
                    return -1;
                }
                mp->audio_codec_ctx = avcodec_alloc_context3(aCodec);
                if (!mp->audio_codec_ctx) {
                    fprintf(stderr, "Failed to allocate codec context.\n");
                    return -1;
                }

                if (avcodec_parameters_to_context(mp->audio_codec_ctx, mp->fmt_ctx->streams[i]->codecpar) < 0) {
                    fprintf(stderr, "Error turning codec params from fmt_ctx to codec_ctx.\n");
                    return -1;
                }
                if (avcodec_open2(mp->audio_codec_ctx, aCodec, NULL) != 0) {
                    fprintf(stderr, "Unable to open the codec.\n");
                    return -1;
                }
                swr_alloc_set_opts2(&mp->resampler_ctx,
                                                       &mp->audio_codec_ctx->ch_layout, AV_SAMPLE_FMT_S16,               mp->audio_codec_ctx->sample_rate,
                                                       &mp->audio_codec_ctx->ch_layout, mp->audio_codec_ctx->sample_fmt, mp->audio_codec_ctx->sample_rate,
                                                       0, NULL);
                if (mp->resampler_ctx == NULL) {
                    fprintf(stderr, "Error allocating swr options.\n");
                    return -1;
                }
                if (swr_init(mp->resampler_ctx) < 0) {
                    fprintf(stderr, "Error initializing swr context.\n");
                    return -1;
                }
                break;
            case AVMEDIA_TYPE_VIDEO:
                mp->video_stream_id = i;
                mp->video_tid = SDL_CreateThread(video_decoder, "video-decoder", mp);
                const AVCodec *vCodec = avcodec_find_decoder(mp->fmt_ctx->streams[i]->codecpar->codec_id);
                if (vCodec == NULL) {
                    fprintf(stderr, "Unable to find the decoder.\n");
                    return -1;
                }
                mp->video_codec_ctx = avcodec_alloc_context3(vCodec);
                if (!mp->video_codec_ctx) {
                    fprintf(stderr, "Failed to allocate codec context.\n");
                    return -1;
                }
                if (avcodec_parameters_to_context(mp->video_codec_ctx, mp->fmt_ctx->streams[i]->codecpar) < 0) {
                    fprintf(stderr, "Error turning codec params from fmt_ctx to codec_ctx.\n");
                    return -1;
                }
                if (avcodec_open2(mp->video_codec_ctx, vCodec, NULL) != 0) {
                    fprintf(stderr, "Unable to open the codec.\n");
                    return -1;
                }
                break;
            default:
                break;
        }
    }

    return 0;
}

int decoder_thread(void *arg) {
    MediaPlayerState *mp = (MediaPlayerState *)arg;
    AVPacket pkt;

    while (1) {
        if (av_read_frame(mp->fmt_ctx, &pkt) < 0) {
            printf("Read total %d packets.\n", mp->video_pkt_queue.nb_packets);
            printf("No more packets to read from the source.\n");
            break;
        }

        if (pkt.stream_index == mp->video_stream_id) {
            if (pkt_queue_put(&mp->video_pkt_queue, &pkt) != 0) {
                break;
            };
        }

        if (pkt.stream_index == mp->audio_stream_id) {
            if (pkt_queue_put(&mp->audio_pkt_queue, &pkt) != 0) {
                break;
            };
        }
    }

    return 0;
}

int video_decoder(void *arg) {
    MediaPlayerState *m = (MediaPlayerState *)arg;
    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();

    while (1) {
        // Should hang the thread until a new pkt is received, unless there are no more packets left in which case this should return with -1
        if (pkt_queue_get(&m->video_pkt_queue, &pkt, m) != 0) {
            break;
        }

        if (avcodec_send_packet(m->video_codec_ctx, &pkt) != 0) {
            fprintf(stderr, "Failed to send the packet to the decoder.\n");
            return -1;
        }

        while (avcodec_receive_frame(m->video_codec_ctx, frame) == 0) {
            SDL_LockMutex(m->framebuffer_mutex);
            while (1) {
                if (m->framebuffer[m->frame_write_index] != NULL) {
                    m->framebuffer[m->frame_write_index] = frame;
                    m->frame_write_index = (m->frame_write_index + 1) % VIDEO_FRAME_BUFFER_SIZE;
                    break;
                } else {
                    printf("Frame buffer full, waiting for space in the buffer - %d\n.", m->frame_write_index);
                    SDL_CondWait(m->framebuffer_cond, m->framebuffer_mutex);
                    printf("Frame buffer now has space, attempting to a new frame \n.");
                } 
            }
            SDL_UnlockMutex(m->framebuffer_mutex);
        }
    }
    return -1;
}