#include "lib/decoder.c"

int main(int argc, char *argv[]) {
    // if (argc < 2) {
    //     fprintf(stderr, "Usage: witch <video file path>");
    //     return -1;
    // }
    const char *input = "av2.mp4";

    MediaPlayerState *mp = alloc_media_player_state();
    if (mp == NULL) {
        return -1;
    }

    if (open_codec(input, mp) != 0) {
        return -1;
    }

    return 0;
}