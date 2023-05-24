#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

typedef struct MediaPlayerState {
    AVFormatContext *fmt_ctx;
    AVCodecContext *video_codec_ctx, *audio_codec_ctx;
    SwrContext *resampler_ctx;
    int video_stream_id, audio_stream_id;
    int audio_device_id;
} MediaPlayerState;

MediaPlayerState* alloc_media_player_state() {
    MediaPlayerState *m = (MediaPlayerState *)av_mallocz(sizeof(MediaPlayerState));
    if (!m) {
        fprintf(stderr, "Failed to allocate memory for the media player.\n");
        return NULL;
    }
    return m;
}