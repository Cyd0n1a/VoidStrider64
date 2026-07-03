#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t score;
    int      mult;
    int      lives;
    bool     gameover;
    bool     title;
    bool     paused;
    float    invuln;     /* seconds of respawn i-frames remaining */
    float    bomb;       /* smart bomb charge 0..1 */
} hud_state_t;

/* Draw score/multiplier/lives text and the game-over overlay. Call at
 * the end of the frame, after all t3d passes (GDD 9.1 step 4 UI). */
void render_ui_draw(const hud_state_t *hud, float time);
