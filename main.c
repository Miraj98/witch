#include "lib/decoder.c"
#include "lib/output.c"

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

    if (setup_sdl(mp) != 0) {
        return -1;
    }


    mp->decoder_tid = SDL_CreateThread(decoder_thread, "decoder-thread", mp);
    // SDL_AddTimer(16, display_frame, (void *)mp);

    while (!mp->quit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    mp->quit = 1;
                    break;
                case REFRESH_VIDEO_DISPLAY:
                    display_frame(mp);
                default:
                    break;
            }
        }
    }

    SDL_DestroyRenderer(mp->display->renderer);
    SDL_DestroyWindow(mp->display->window);
    SDL_Quit();

    return 0;
}