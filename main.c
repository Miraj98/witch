#include "lib/decoder.c"

int main(int argc, char *argv[]) {
    // if (argc < 2) {
    //     fprintf(stderr, "Usage: witch <video file path>");
    //     return -1;
    // }
    SDL_Event event;
    const char *input = "av2.mp4";
    MediaPlayerState *mp = alloc_media_player_state();
    if (mp == NULL) {
        return -1;
    }

    if (open_codec(input, mp) != 0) {
        return -1;
    }

    mp->decoder_tid = SDL_CreateThread(decoder_thread, "decoder-thread", mp);

    for (;;) {
        SDL_WaitEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                mp->quit = 1;
                break;
            default:
                break;
        }
    }

    return 0;
}