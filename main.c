#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL.h>

#define MAX_AUDIO_FRAME_SIZE 192000

int main(int argc, char *argv[]) {
    // if (argc < 2) {
    //     printf("Please provide a video file.\n");
    //     return -1;
    // }

    // const char *videoFile = argv[1];
    const char *videoFile = "av2.mp4";


    AVFormatContext *formatCtx = NULL;
    if (avformat_open_input(&formatCtx, videoFile, NULL, NULL) != 0) {
        printf("Failed to open video file.\n");
        return -1;
    }

    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        printf("Failed to retrieve stream information.\n");
        avformat_close_input(&formatCtx);
        return -1;
    }

    int audioStream = -1;
    AVCodecParameters *codecParams = NULL;
    AVCodec *codec = NULL;

    // Find the audio stream
    for (int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
            codecParams = formatCtx->streams[i]->codecpar;
            codec = avcodec_find_decoder(codecParams->codec_id);
            break;
        }
    }

    if (audioStream == -1) {
        printf("No audio stream found in the video file.\n");
        avformat_close_input(&formatCtx);
        return -1;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == NULL) {
        printf("Failed to allocate codec context.\n");
        avformat_close_input(&formatCtx);
        return -1;
    }

    if (avcodec_parameters_to_context(codecCtx, codecParams) < 0) {
        printf("Failed to copy codec parameters to codec context.\n");
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        printf("Failed to open codec.\n");
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    AVPacket *packet = av_packet_alloc();
    if (packet == NULL) {
        printf("Failed to allocate packet.\n");
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    AVFrame *frame = av_frame_alloc();
    if (frame == NULL) {
        printf("Failed to allocate frame.\n");
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    // Set up audio resampling
    // SwrContext *swrCtx = swr_alloc_set_opts(NULL,
    //     AV_CH_LAYOUT_STEREO,
    //     AV_SAMPLE_FMT_S16,
    //     codecCtx->sample_rate,
    //     codecCtx->channel_layout,
    //     codecCtx->sample_fmt,
    //     codecCtx->sample_rate,
    //     0,
    //     NULL);
        SwrContext *swrCtx = swr_alloc_set_opts(NULL,
                                            codecCtx->channel_layout, AV_SAMPLE_FMT_S16,    codecCtx->sample_rate,
                                            codecCtx->channel_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
                                            0, NULL);
    if (swrCtx == NULL) {
        printf("Failed to allocate resampler context.\n");
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    if (swr_init(swrCtx) < 0) {
        printf("Failed to initialize resampler.\n");
        swr_free(&swrCtx);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    // Initialize SDL audio
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("Failed to initialize SDL audio.\n");
        swr_free(&swrCtx);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    SDL_AudioSpec wantedSpec, obtainedSpec;
    memset(&wantedSpec, 0, sizeof(wantedSpec));
    memset(&obtainedSpec, 0, sizeof(obtainedSpec));

    wantedSpec.freq = codecCtx->sample_rate;
    wantedSpec.format = AUDIO_S16SYS;
    wantedSpec.channels = codecCtx->channels;
    wantedSpec.silence = 0;
    wantedSpec.samples = codecCtx->frame_size;
    wantedSpec.callback = NULL;

    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &wantedSpec, &obtainedSpec, 0);
    if (audioDevice == 0) {
        printf("Failed to open audio device: %s\n", SDL_GetError());
        SDL_Quit();
        swr_free(&swrCtx);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    // Initialize audio buffer
    uint8_t *audioBuffer = (uint8_t *) malloc(MAX_AUDIO_FRAME_SIZE);
    if (audioBuffer == NULL) {
        printf("Failed to allocate audio buffer.\n");
        SDL_CloseAudioDevice(audioDevice);
        SDL_Quit();
        swr_free(&swrCtx);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return -1;
    }

    SDL_PauseAudioDevice(audioDevice, 0);

    // Decode audio frames
    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == audioStream) {
            int ret = avcodec_send_packet(codecCtx, packet);
            if (ret < 0) {
                printf("Error sending a packet for decoding.\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0) {
                    printf("Error during decoding.\n");
                    break;
                }

                // Resample audio data
                uint8_t *out[] = {audioBuffer};
                int outSamples = swr_convert(swrCtx, out, frame->nb_samples, (const uint8_t **) frame->data, frame->nb_samples);

                // Write audio data to SDL
                int audioSize = outSamples * codecCtx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                SDL_QueueAudio(audioDevice, audioBuffer, audioSize);
            }
        }

        av_packet_unref(packet);
    }

    // Flush the decoder
    avcodec_send_packet(codecCtx, NULL);
    while (avcodec_receive_frame(codecCtx, frame) == 0) {
        uint8_t *out[] = {audioBuffer};
        int outSamples = swr_convert(swrCtx, out, frame->nb_samples, (const uint8_t **) frame->data, frame->nb_samples);
        int audioSize = outSamples * codecCtx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        SDL_QueueAudio(audioDevice, audioBuffer, audioSize);
    }

    SDL_Delay(100000); // Wait for the audio to finish playing
    SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();
    swr_free(&swrCtx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);

    return 0;
}
