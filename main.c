#include "libavcodec/codec.h"
#include "libavcodec/codec_par.h"
#include "libavcodec/packet.h"
#include "libavutil/avutil.h"
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

int get_codec(
    const char* input,
    enum AVMediaType codec_type,
    int* stream_idx,
    AVFormatContext** fc,
    AVCodecContext** codec_ctx
);

int main() {
    AVFormatContext* format_context = NULL;
    AVCodecContext* codec_ctx = NULL;
    int stream_idx = -1;

    int ret = get_codec("richard-feynman.mp4", AVMEDIA_TYPE_VIDEO,
                        &stream_idx, &format_context, &codec_ctx);
    if (ret < 0) { return ret; }

    AVPacket pkt;
    AVFrame* frame = av_frame_alloc();
    int iter = 0;

    printf("Pixel format %d -> %d\n", codec_ctx->pix_fmt, AV_PIX_FMT_YUV420P);

    while (av_read_frame(format_context, &pkt) >= 0) {
        if (pkt.stream_index == stream_idx) {
            if (avcodec_send_packet(codec_ctx, &pkt) < 0) {
                fprintf(stderr, "Error sending packet to the codec\n");
                return -1;
            }

            while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                // Do something with the frame
            }
        }
    }


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

    printf("This the pixel format -> %d", codec_params->format);

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
