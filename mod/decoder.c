#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

int open_codec(const char *filepath, enum AVMediaType media_type,
                AVFormatContext **format_ctx, AVCodecContext **codec_ctx)
{
    int stream_id = -1;

    if ((*format_ctx) == NULL) {
        if (avformat_open_input(format_ctx, filepath, NULL, NULL) != 0) {
            fprintf(stderr, "Error opening the input.\n");
            return -1;
        }

        if (avformat_find_stream_info(*format_ctx, NULL) < 0) {
            fprintf(stderr, "Error finding stream info.\n");
            return -1;
        }
    }

    const AVCodec *codec = NULL;
    for (int i = 0; i < (*format_ctx)->nb_streams; i++) {
        if ((*format_ctx)->streams[i]->codecpar->codec_type == media_type) {
            stream_id = i;
            int codecid = (*format_ctx)->streams[i]->codecpar->codec_id;
            codec = avcodec_find_decoder(codecid);
            if (codec == NULL) {
                fprintf(stderr, "Unable to find the decoder.\n");
                return -1;
            }
            *codec_ctx = avcodec_alloc_context3(codec);
            if (!*codec_ctx) {
                fprintf(stderr, "Unable to alloc codec context.\n");
                return -1;
            }
            if (avcodec_parameters_to_context(*codec_ctx, (*format_ctx)->streams[i]->codecpar) < 0) {
                fprintf(stderr, "Error turning params to context.\n");
                return -1;
            }
            break;
        }
    }

    if (avcodec_open2(*codec_ctx, codec, NULL) != 0) {
        fprintf(stderr, "Unable to open the codec.\n");
        return -1;
    }

    return stream_id;
}