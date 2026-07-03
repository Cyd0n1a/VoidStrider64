#include "render_ui.h"
#include <libdragon.h>

void render_ui_draw(const hud_state_t *hud, float time) {
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 16, 20,
                     "%07lu  x%d", (unsigned long)hud->score, hud->mult);
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 250, 20,
                     "SHIPS %d", hud->lives < 0 ? 0 : hud->lives);
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 276, 230,
                     "%.0ffps", display_get_fps());

    if (hud->gameover) {
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 124, 110, "GAME OVER");
        if ((int)(time * 2.f) & 1)
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 112, 130,
                             "PRESS START");
    }
}
