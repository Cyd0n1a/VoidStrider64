#include "render_ui.h"
#include "../gen/palette_gen.h"
#include "../meta/options.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>

/* ---- Big title lettering ----
 * The builtin debug font can't scale, so the title is drawn from a tiny
 * 5x7 glyph table as RDP fill rects: ~15 rects per letter, trivially
 * per-letter colored and displaced. Only the characters the title needs. */
#define GLYPH_W   5
#define GLYPH_H   7
#define TITLE_SCALE 3

typedef struct { char c; uint8_t rows[GLYPH_H]; } glyph_t;

static const glyph_t GLYPHS[] = {
    { 'V', { 0x11,0x11,0x11,0x11,0x0A,0x0A,0x04 } },
    { 'O', { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E } },
    { 'I', { 0x1F,0x04,0x04,0x04,0x04,0x04,0x1F } },
    { 'D', { 0x1E,0x11,0x11,0x11,0x11,0x11,0x1E } },
    { 'S', { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E } },
    { 'T', { 0x1F,0x04,0x04,0x04,0x04,0x04,0x04 } },
    { 'R', { 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11 } },
    { 'E', { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F } },
    { '6', { 0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E } },
    { '4', { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 } },
};

static const uint8_t *glyph_rows(char c) {
    for (unsigned i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); i++)
        if (GLYPHS[i].c == c) return GLYPHS[i].rows;
    return NULL;
}

static void draw_wavy_title(float time) {
    const char *title = "VOIDSTRIDER64";
    int n  = (int)strlen(title);
    int cw = (GLYPH_W + 1) * TITLE_SCALE;            /* advance per char */
    int x0 = (320 - (n * cw - TITLE_SCALE)) / 2;

    rdpq_mode_push();
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 0));
    for (int i = 0; i < n; i++) {
        const uint8_t *rows = glyph_rows(title[i]);
        if (!rows) continue;

        /* Rainbow flows along the word; each letter bobs on its own
         * phase of a gentle sine. */
        uint32_t c = palette_hsv_rgba(time * 0.18f + (float)i * 0.08f,
                                      0.9f, 1.0f);
        rdpq_set_fill_color(RGBA32((c >> 24) & 0xFF, (c >> 16) & 0xFF,
                                   (c >> 8) & 0xFF, 255));
        int lx = x0 + i * cw;
        int ly = 58 + (int)(fm_sinf(time * 2.3f + (float)i * 0.55f) * 5.f);

        for (int r = 0; r < GLYPH_H; r++) {
            for (int b = 0; b < GLYPH_W; b++) {
                if (!(rows[r] & (0x10 >> b))) continue;
                int px = lx + b * TITLE_SCALE;
                int py = ly + r * TITLE_SCALE;
                rdpq_fill_rectangle(px, py, px + TITLE_SCALE, py + TITLE_SCALE);
            }
        }
    }
    rdpq_mode_pop();
}

/* ---- Scrolling credits, lower third ---- */
static const char *CREDITS[] = {
    "GAME DESIGN + CODE",
    "AMANDA HARIETTE-SCOTT",
    "",
    "MUSIC + SOUNDTRACK",
    "AMANDA HARIETTE-SCOTT",
    "CYDONIS HEAVY INDUSTRIES",
    "",
    "BUILT WITH",
    "LIBDRAGON - DRAGONMINDED + CONTRIBUTORS",
    "TINY3D - HAILTODODONGO",
    "XM PLAYBACK - LIBXM BY ARTEFACT2",
    "",
    "",
    "INSPIRED BY",
    "GEOMETRY WARS - BIZARRE CREATIONS",
    "SPACE GIRAFFE - LLAMASOFT",
    "",
    "(C) 2026 AMANDA HARIETTE-SCOTT",
    "AND CYDONIS HEAVY INDUSTRIES (cydonis.co.uk)",
    "",
    "P.S - Trans rights are human rights!",
    "Made with love, on planet Earth!",
};
#define CREDIT_LINES (int)(sizeof(CREDITS) / sizeof(CREDITS[0]))
#define CREDIT_LH    10      /* line height, px */
#define CREDIT_TOP   168     /* lower-third band */
#define CREDIT_BOT   234

static void draw_credits(float time) {
    int   region = CREDIT_BOT - CREDIT_TOP;
    float period = (float)(CREDIT_LINES * CREDIT_LH + region);
    float scroll = fmodf(time * 13.f, period);

    for (int i = 0; i < CREDIT_LINES; i++) {
        int y = CREDIT_BOT + i * CREDIT_LH - (int)scroll;
        if (y < CREDIT_TOP || y > CREDIT_BOT - 8) continue;
        int len = (int)strlen(CREDITS[i]);
        if (!len) continue;
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO,
                         160 - len * 4, y, "%s", CREDITS[i]);
    }
}

static void draw_options(const hud_state_t *hud, float time) {
    (void)time;
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 132, 70, "OPTIONS");

    const int y0 = 100, lh = 14;
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 70, y0,
                     "BACKGROUND FX   %3d%%",
                     (int)(g_options.bg_intensity * 100.f + 0.5f));
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 70, y0 + lh,
                     "REDUCE FLASH    %s",
                     g_options.reduce_flash ? "ON" : "OFF");
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 70, y0 + 2 * lh,
                     "EDIT SEEDS");
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 70, y0 + 3 * lh,
                     "BACK");
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 54, y0 + hud->cursor * lh,
                     ">");

    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 52, 190,
                     "D-PAD: NAVIGATE/ADJUST  A: SELECT  B: BACK");
    if (!hud->save_ok)
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 76, 210,
                         "! NO EEPROM - NOTHING WILL SAVE !");
}

static void draw_seeds(const hud_state_t *hud, float time) {
    (void)time;
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 140, 70, "SEEDS");

    const int y0 = 104, lh = 22, hex_x = 148;
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 52, y0,
                     "COSMETIC    %08lX", (unsigned long)hud->cseed);
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 52, y0 + lh,
                     "DIFFICULTY  %08lX", (unsigned long)hud->dseed);

    /* Caret under the digit being edited (8 hex digits per row). */
    int row = hud->cursor / 8, digit = hud->cursor % 8;
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO,
                     hex_x + digit * 8, y0 + row * lh + 9, "^");

    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 44, 176,
                     "D-PAD: MOVE/CHANGE DIGIT  B: BACK");
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 44, 190,
                     "SEEDS APPLY TO THE NEXT RUN");
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 44, 204,
                     "SHARE THEM TO SHARE THE RUN");
}

void render_ui_draw(const hud_state_t *hud, float time) {
    if (hud->screen == SCR_TITLE) {
        draw_wavy_title(time);
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 124, 122,
                         "HI %07lu", (unsigned long)hud->hi_score);
        if ((int)(time * 2.f) & 1)
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 116, 140,
                             "PRESS START");
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 116, 154,
                         "A: OPTIONS");
        draw_credits(time);
        return;
    }
    if (hud->screen == SCR_OPTIONS) { draw_options(hud, time); return; }
    if (hud->screen == SCR_SEEDS)   { draw_seeds(hud, time);   return; }

    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 16, 20,
                     "%07lu  x%d", (unsigned long)hud->score, hud->mult);
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 250, 20,
                     "SHIPS %d", hud->lives < 0 ? 0 : hud->lives);
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 276, 230,
                     "%.0ffps", display_get_fps());

    if (hud->screen == SCR_PAUSE) {
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 136, 116, "PAUSED");
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 76, 132,
                         "D-PAD L/R: BACKGROUND FX %3d%%",
                         (int)(g_options.bg_intensity * 100.f + 0.5f));
    }

    if (hud->screen == SCR_GAMEOVER) {
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 124, 96, "GAME OVER");
        rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 96, 112,
                         "TIME %lus  SEED %08lX",
                         (unsigned long)hud->run_secs,
                         (unsigned long)hud->dseed);
        if (hud->hs_rank >= 0)
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 92, 128,
                             "NEW HIGH SCORE - RANK %d", hud->hs_rank + 1);
        if ((int)(time * 2.f) & 1)
            rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 112, 148,
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
