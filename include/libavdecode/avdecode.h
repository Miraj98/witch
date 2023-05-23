#include <libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libavutil/avutil.h>

int open_codec(const char * filepath, enum AVMediaType media_type,
                AVFormatContext **format_ctx, AVCodecContext **codec_ctx);