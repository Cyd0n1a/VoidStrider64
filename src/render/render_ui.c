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

    /* Bomb charge meter, bottom-left (Z to fire when full). */
    rdpq_mode_push();
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 0));
    rdpq_set_fill_color(RGBA32(26, 26, 40, 255));
    rdpq_fill_rectangle(16, 224, 16 + 70, 231);
    int fill = (int)(hud->bomb * 70.f);
    if (fill > 0) {
        color_t c = (hud->bomb >= 1.f)
            ? ((int)(time * 4.f) & 1 ? RGBA32(120, 255, 255, 255)
                                     : RGBA32(60, 200, 230, 255))
            : RGBA32(190, 120, 40, 255);
        rdpq_set_fill_color(c);
        rdpq_fill_rectangle(16, 224, 16 + fill, 231);
    }
    rdpq_mode_pop();
}
