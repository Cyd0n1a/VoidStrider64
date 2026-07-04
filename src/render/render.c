#include "render.h"
#include "render_entities.h"
#include "render_ui.h"
#include "splash.h"
#include "../gen/tunnel_gen.h"
#include "../gen/grid_sim.h"
#include "../gen/mesh_gen.h"
#include "../sim/arena.h"
#include "../audio/synth.h"
#include "../meta/options.h"
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <libdragon.h>

/* Fixed baseline seed for the ship silhouette (GDD 5.2). */
#define SHIP_SEED 0x51A917E5u

static T3DViewport viewport;
static surface_t   zbuf;

/* Double-buffered: the RSP may still be DMA-ing frame N-1's verts/matrices
 * while the CPU writes frame N, so alternate between two sets. Each rspq
 * block bakes in its buffer's addresses; contents are re-read every run. */
static T3DVertPacked *tunnel_verts[2];
static T3DMat4FP     *tunnel_mat[2];
static rspq_block_t  *tunnel_dpl[2];

static T3DVertPacked *grid_verts[2];
static rspq_block_t  *grid_dpl[2];
static T3DMat4FP     *ident_mat;       /* grid lives in world space */

static T3DVertPacked *ship_verts[2];
static T3DMat4FP     *ship_mat[2];
static rspq_block_t  *ship_dpl[2];

static int frame_idx;

/* With 3 framebuffers the CPU can run up to two frames ahead of the RSP,
 * so two vert/matrix buffer sets alone don't guarantee the RSP finished
 * frame N before we rewrite its buffers for frame N+2. Wait on a
 * syncpoint per buffer set before reusing it (fixes torn/spiky verts). */
static rspq_syncpoint_t buf_sync[2];

/* Frame-budget director (GDD 9.2): when the frame trends over 60fps
 * budget, the tunnel window shrinks first — gameplay never degrades. */
#define TUNNEL_RINGS_LOW 15
#define TUNNEL_SEGS_LOW  (TUNNEL_RINGS_LOW - 1)
static rspq_block_t *tunnel_dpl_low[2];
static int   fb_level;
static float fb_avg_ms;
static int   fb_good;
static long long fb_prev;

float render_frame_ms(void)  { return fb_avg_ms; }
bool  render_fb_reduced(void) { return fb_level != 0; }

static rspq_block_t *record_tunnel_dpl(T3DVertPacked *verts, T3DMat4FP *mat,
                                       int segs) {
    rspq_block_t *dpl;
    rspq_block_begin();
        t3d_matrix_push(mat);
        for (int s = 0; s < segs; s++) {
            /* Load ring s and ring s+1 (contiguous, slot-ordered buffer),
             * then stitch them with a quad per side. No cull flags are set,
             * so winding doesn't matter for the inside-viewed tube. */
            t3d_vert_load(verts + s * TUNNEL_PACKED_PER_RING, 0, TUNNEL_SIDES * 2);
            for (int j = 0; j < TUNNEL_SIDES; j++) {
                int a = j;
                int b = (j + 1) % TUNNEL_SIDES;
                int c = TUNNEL_SIDES + j;
                int d = TUNNEL_SIDES + (j + 1) % TUNNEL_SIDES;
                t3d_tri_draw(a, c, b);
                t3d_tri_draw(b, c, d);
            }
            t3d_tri_sync();
        }
        t3d_matrix_pop(1);
    dpl = rspq_block_end();
    return dpl;
}

static rspq_block_t *record_grid_dpl(T3DVertPacked *verts) {
    rspq_block_t *dpl;
    rspq_block_begin();
        t3d_matrix_push(ident_mat);
        for (int r = 0; r < GRID_ROWS; r++) {
            t3d_vert_load(verts + r * GRID_PACKED_PER_BATCH, 0, GRID_VERTS_PER_BATCH);
            /* Slots: 0..12 row r, 13..25 row r+1, 26..37 cell centers.
             * Four-triangle fan per cell around the dark center vert. */
            for (int c = 0; c < GRID_COLS; c++) {
                int a = c, b = c + 1;
                int e = GRID_IX_W + c, f = GRID_IX_W + c + 1;
                int m = 2 * GRID_IX_W + c;
                t3d_tri_draw(a, b, m);
                t3d_tri_draw(b, f, m);
                t3d_tri_draw(f, e, m);
                t3d_tri_draw(e, a, m);
            }
            t3d_tri_sync();
        }
        t3d_matrix_pop(1);
    dpl = rspq_block_end();
    return dpl;
}

static rspq_block_t *record_ship_dpl(T3DVertPacked *verts, T3DMat4FP *mat) {
    rspq_block_t *dpl;
    rspq_block_begin();
        t3d_matrix_push(mat);
        t3d_vert_load(verts, 0, SHIP_TOTAL_VERTS);
        mesh_ship_draw_hull();
        t3d_tri_draw(6, 7, 8);      /* thruster */
        t3d_tri_sync();
        t3d_matrix_pop(1);
    dpl = rspq_block_end();
    return dpl;
}

/* Thruster flame: length tracks speed, flickers procedurally (GDD 5.2 —
 * the one animated part of the ship's geometry). Verts 6..8. */
static void write_thruster(T3DVertPacked *verts, const player_t *p, float time) {
    float flick = 0.85f + 0.10f * fm_sinf(time * 37.f)
                        + 0.05f * fm_sinf(time * 23.3f);
    float len = (5.f + 20.f * p->speed_norm) * flick;

    T3DVertPacked *pk = &verts[3];   /* verts 6,7 */
    pk->posA[0] = (int16_t)SHIP_THRUST_BASE_L;
    pk->posA[1] = (int16_t)SHIP_THRUST_BASE_Y;
    pk->posA[2] = 0; pk->normA = 0; pk->rgbaA = 0xFF9C28FF;
    pk->posB[0] = (int16_t)SHIP_THRUST_BASE_R;
    pk->posB[1] = (int16_t)SHIP_THRUST_BASE_Y;
    pk->posB[2] = 0; pk->normB = 0; pk->rgbaB = 0xFF9C28FF;

    pk = &verts[4];                  /* vert 8 (+ pad 9) */
    pk->posA[0] = 0;
    pk->posA[1] = (int16_t)(SHIP_THRUST_BASE_Y - len);
    pk->posA[2] = 0; pk->normA = 0; pk->rgbaA = 0xFF3C00FF;
}

void render_init(void) {
    viewport = t3d_viewport_create();
    zbuf     = surface_alloc(FMT_RGBA16, 320, 240);

    ident_mat = malloc_uncached(sizeof(T3DMat4FP));
    t3d_mat4fp_identity(ident_mat);

    for (int i = 0; i < 2; i++) {
        tunnel_verts[i] = malloc_uncached(sizeof(T3DVertPacked) * TUNNEL_PACKED_COUNT);
        tunnel_mat[i]   = malloc_uncached(sizeof(T3DMat4FP));
        t3d_mat4fp_identity(tunnel_mat[i]);
        tunnel_dpl[i]     = record_tunnel_dpl(tunnel_verts[i], tunnel_mat[i], TUNNEL_SEGS);
        tunnel_dpl_low[i] = record_tunnel_dpl(tunnel_verts[i], tunnel_mat[i], TUNNEL_SEGS_LOW);

        grid_verts[i]   = malloc_uncached(sizeof(T3DVertPacked) * GRID_PACKED_COUNT);
        grid_dpl[i]     = record_grid_dpl(grid_verts[i]);

        ship_verts[i]   = malloc_uncached(sizeof(T3DVertPacked) * SHIP_PACKED_COUNT);
        ship_mat[i]     = malloc_uncached(sizeof(T3DMat4FP));
        t3d_mat4fp_identity(ship_mat[i]);
        mesh_gen_ship(ship_verts[i], SHIP_SEED);
        ship_dpl[i]     = record_ship_dpl(ship_verts[i], ship_mat[i]);
    }
    frame_idx = 0;

    render_entities_init();

    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO, font);
    /* Style 1: yellow, for the MOTD marquee (render_ui.c). */
    rdpq_font_style(font, 1, &(rdpq_fontstyle_t){
        .color = RGBA32(255, 220, 40, 255),
    });
}

void render_frame(surface_t *disp, float time, const player_t *player,
                  const hud_state_t *hud) {
    frame_idx ^= 1;
    int fi = frame_idx;

    if (buf_sync[fi])
        rspq_syncpoint_wait(buf_sync[fi]);

    /* Frame-budget director (GDD 9.2): rolling frame-time average with
     * hysteresis. Over budget -> shrink the tunnel window; only restore
     * after 4s of comfortably clean frames. */
    long long now = timer_ticks();
    if (fb_prev) {
        float ms = (float)TIMER_MICROS_LL(now - fb_prev) / 1000.f;
        if (ms < 100.f)
            fb_avg_ms = fb_avg_ms ? fb_avg_ms + (ms - fb_avg_ms) * 0.1f : ms;
        if (fb_level == 0 && fb_avg_ms > 17.5f) {
            fb_level = 1;
            fb_good  = 0;
        } else if (fb_level == 1) {
            fb_good = (fb_avg_ms < 16.9f) ? fb_good + 1 : 0;
            if (fb_good > 240) fb_level = 0;
        }
    }
    fb_prev = now;
    int n_rings = fb_level ? TUNNEL_RINGS_LOW : TUNNEL_RINGS;

    float beat = synth_beat_pulse() * options_flash_scale();
    tunnel_build_verts(tunnel_verts[fi], time, n_rings);
    grid_build_verts(grid_verts[fi], time, beat);
    write_thruster(ship_verts[fi], player, time);
    render_entities_build(fi, time);

    /* Chase-cam roll: rotate the tunnel around the view axis; the camera
     * and gameplay layers stay fixed and screen-relative (GDD 6.2). */
    float r        = tunnel_roll();
    float quat[4]  = { 0.f, 0.f, fm_sinf(r * 0.5f), fm_cosf(r * 0.5f) };
    float scale1[3] = { 1.f, 1.f, 1.f };
    float pos0[3]   = { 0.f, 0.f, 0.f };
    t3d_mat4fp_from_srt(tunnel_mat[fi], scale1, quat, pos0);

    /* Ship: model forward is +Y == heading pi/2, so rotate by the delta. */
    float ha = player->heading - 1.5707963f;
    float squat[4] = { 0.f, 0.f, fm_sinf(ha * 0.5f), fm_cosf(ha * 0.5f) };
    float spos[3]  = { player->x, player->y, ARENA_Z };
    t3d_mat4fp_from_srt(ship_mat[fi], scale1, squat, spos);

    T3DVec3 cam_pos = {{ 0.f, 0.f,    0.f }};
    T3DVec3 cam_tgt = {{ 0.f, 0.f, -100.f }};
    T3DVec3 up      = {{ 0.f, 1.f,    0.f }};
    t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(78.f), 8.f, 1900.f);
    t3d_viewport_look_at(&viewport, &cam_pos, &cam_tgt, &up);

    rdpq_attach(disp, &zbuf);
    t3d_frame_start();
    t3d_viewport_attach(&viewport);
    t3d_screen_clear_color(RGBA32(3, 2, 8, 255));
    t3d_screen_clear_depth();

    /* --- Layer 1: wormhole tunnel (pure vertex color, self-lit) --- */
    t3d_light_set_count(0);
    t3d_state_set_drawflags(T3D_FLAG_SHADED | T3D_FLAG_DEPTH | T3D_FLAG_NO_LIGHT);
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rspq_block_run(fb_level ? tunnel_dpl_low[fi] : tunnel_dpl[fi]);

    /* Layer compositing (GDD 5.1): background must never occlude the
     * playfield, so wipe depth between layers instead of sharing Z. */
    t3d_screen_clear_depth();

    /* --- Layer 2: transparent grid membrane (alpha blended) ---
     * A single plane: it neither tests nor writes Z, so gameplay can
     * never lose a depth contest against it (entities sit at the same
     * arena Z, enemies even extend behind it). */
    rdpq_mode_zbuf(false, false);
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
    rspq_block_run(grid_dpl[fi]);

    /* --- Layer 3: gameplay, opaque on top (GDD 5.1). Menu screens
     * (title/options/seeds) show only tunnel + grid. --- */
    rdpq_mode_blender(0);
    if (hud->screen >= SCR_PLAY) {
        render_entities_draw(fi, time);

        /* Ship last, Z still off (left so by the entities pass): the
         * player always reads on top of everything (GDD 1.1 #3). Blink
         * at 8Hz during respawn i-frames; hidden on game over. */
        bool blink = hud->invuln > 0.f && ((int)(time * 8.f) & 1);
        if (hud->screen != SCR_GAMEOVER && !blink)
            rspq_block_run(ship_dpl[fi]);
    }

    render_ui_draw(hud, time);

    /* Boot splash fading out over the title scene (no-op once done). */
    if (hud->screen == SCR_TITLE)
        splash_draw_overlay();

    rdpq_detach_show();
    buf_sync[fi] = rspq_syncpoint_new();
}
