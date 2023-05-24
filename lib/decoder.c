#include "typedefs.c"
#include <libswresample/swresample.h>

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