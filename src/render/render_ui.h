#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SCR_TITLE,
    SCR_OPTIONS,
    SCR_SEEDS,
    SCR_PLAY,      /* gameplay layers render from here on */
    SCR_PAUSE,
    SCR_GAMEOVER,
} screen_t;

typedef struct {
    screen_t screen;
    uint32_t score;
    int      mult;
    int      lives;
    float    invuln;     /* seconds of respawn i-frames remaining */
    float    bomb;       /* smart bomb charge 0..1 */
    /* menus */
    int      cursor;     /* options item / seed digit index */
    uint32_t cseed, dseed;
    /* meta */
    uint32_t hi_score;
    int      hs_rank;    /* 0-based rank of a fresh game-over run, -1 none */
    uint32_t run_secs;
    bool     save_ok;
    const char *motd;    /* fortune for the pause/game-over marquee */
} hud_state_t;

/* Draw HUD/menu text and the game-over overlay. Call at the end of the
 * frame, after all t3d passes (GDD 9.1 step 4 UI). */
void render_ui_draw(const hud_state_t *hud, float time);
