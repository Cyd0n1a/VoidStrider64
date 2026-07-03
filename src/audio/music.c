#include "music.h"
#include <libdragon.h>

/* Mixer channel plan: wav64 music on 0 (+1 when stereo); the 24-channel
 * title XM on 2..25; SFX synth uses no mixer channels at all. */
#define CH_WAV  0
#define CH_XM   2

typedef enum { M_OFF, M_TITLE, M_GAME } music_mode_t;

static xm64player_t xm;
static wav64_t      wav;
static music_mode_t mode;

void music_init(void) {
    xm64player_open(&xm, "rom:/title.xm64");
    xm64player_set_loop(&xm, true);
    wav64_open(&wav, "rom:/gameplay.wav64");
    wav64_set_loop(&wav, true);
    mode = M_OFF;
}

void music_stop(void) {
    if (mode == M_TITLE)
        xm64player_stop(&xm);
    if (mode == M_GAME) {
        mixer_ch_stop(CH_WAV);
        mixer_ch_stop(CH_WAV + 1);
    }
    mode = M_OFF;
}

void music_title(void) {
    if (mode == M_TITLE) return;
    music_stop();
    xm64player_seek(&xm, 0, 0, 0);   /* restart from the top */
    xm64player_play(&xm, CH_XM);
    mode = M_TITLE;
}

void music_gameplay(void) {
    if (mode == M_GAME) return;
    music_stop();
    wav64_play(&wav, CH_WAV);
    mode = M_GAME;
}
