#include "render_entities.h"
#include "../gen/mesh_gen.h"
#include "../gen/palette_gen.h"
#include "../sim/arena.h"
#include "../sim/enemies.h"
#include "../sim/projectiles.h"
#include "../sim/shards.h"
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <libdragon.h>

#define TETRA_VARIANTS   4
#define TETRA_RADIUS     13.f
#define WANDERER_HUE     0.50f   /* class delta off the wheel walk: cyan at hue 0 */

#define BULLET_LEN       7.f
#define BULLET_W         2.5f
#define SHARD_R          5.f

/* Bullets: 4 verts (2 packed) per elongated diamond.
 * Shards: 3 verts + 1 pad (2 packed) per triangle. Inactive slots
 * collapse to the origin, which the near plane clips away. */
#define BULLET_PACKED    (MAX_BULLETS * 2)
#define SHARD_PACKED     (MAX_SHARDS * 2)

static T3DVertPacked *tetra_verts;                 /* 4 variants, static */
static T3DMat4FP     *enemy_mats[2];               /* MAX_ENEMIES each */
static T3DVertPacked *proj_verts[2];               /* bullets then shards */
static rspq_block_t  *proj_dpl[2];
static T3DMat4FP     *ident_mat;

static rspq_block_t *record_proj_dpl(T3DVertPacked *verts) {
    rspq_block_t *dpl;
    rspq_block_begin();
        t3d_matrix_push(ident_mat);
        /* Bullets: chunks of 16 quads = 64 verts per load. */
        for (int base = 0; base < MAX_BULLETS; base += 16) {
            int n = MAX_BULLETS - base; if (n > 16) n = 16;
            t3d_vert_load(verts + base * 2, 0, n * 4);
            for (int q = 0; q < n; q++) {
                t3d_tri_draw(4 * q, 4 * q + 1, 4 * q + 2);
                t3d_tri_draw(4 * q, 4 * q + 2, 4 * q + 3);
            }
            t3d_tri_sync();
        }
        /* Shards: chunks of 16 tris (4 verts each incl. pad). */
        for (int base = 0; base < MAX_SHARDS; base += 16) {
            int n = MAX_SHARDS - base; if (n > 16) n = 16;
            t3d_vert_load(verts + BULLET_PACKED + base * 2, 0, n * 4);
            for (int q = 0; q < n; q++)
                t3d_tri_draw(4 * q, 4 * q + 1, 4 * q + 2);
            t3d_tri_sync();
        }
        t3d_matrix_pop(1);
    dpl = rspq_block_end();
    return dpl;
}

void render_entities_init(void) {
    ident_mat = malloc_uncached(sizeof(T3DMat4FP));
    t3d_mat4fp_identity(ident_mat);

    tetra_verts = malloc_uncached(sizeof(T3DVertPacked) *
                                  TETRA_PACKED_COUNT * TETRA_VARIANTS);
    for (int v = 0; v < TETRA_VARIANTS; v++)
        mesh_gen_tetra(tetra_verts + v * TETRA_PACKED_COUNT,
                       TETRA_RADIUS, 0xBEEF0001u + (uint32_t)v);

    for (int i = 0; i < 2; i++) {
        enemy_mats[i] = malloc_uncached(sizeof(T3DMat4FP) * MAX_ENEMIES);
        for (int e = 0; e < MAX_ENEMIES; e++)
            t3d_mat4fp_identity(&enemy_mats[i][e]);

        proj_verts[i] = malloc_uncached(sizeof(T3DVertPacked) *
                                        (BULLET_PACKED + SHARD_PACKED));
        for (int p = 0; p < BULLET_PACKED + SHARD_PACKED; p++)
            proj_verts[i][p] = (T3DVertPacked){0};
        proj_dpl[i] = record_proj_dpl(proj_verts[i]);
    }
}

static inline void put_vert(T3DVertPacked *buf, int slot,
                            float x, float y, float z, uint32_t col) {
    T3DVertPacked *p = &buf[slot / 2];
    if (slot & 1) {
        p->posB[0] = (int16_t)x; p->posB[1] = (int16_t)y; p->posB[2] = (int16_t)z;
        p->normB = 0; p->rgbaB = col; p->stB[0] = 0; p->stB[1] = 0;
    } else {
        p->posA[0] = (int16_t)x; p->posA[1] = (int16_t)y; p->posA[2] = (int16_t)z;
        p->normA = 0; p->rgbaA = col; p->stA[0] = 0; p->stA[1] = 0;
    }
}

static void collapse_slot4(T3DVertPacked *buf, int first_slot) {
    for (int s = 0; s < 4; s++)
        put_vert(buf, first_slot + s, 0.f, 0.f, 0.f, 0);
}

void render_entities_build(int fi, float time) {
    T3DVertPacked *buf = proj_verts[fi];

    /* --- Bullets: diamond stretched along velocity, warm & fixed-color
     * for readability (GDD 1.1 #3). --- */
    for (int i = 0; i < MAX_BULLETS; i++) {
        int slot = i * 4;
        if (!bullets[i].alive) { collapse_slot4(buf, slot); continue; }
        float inv = 1.f / 500.f;   /* bullets fly at constant speed */
        float dx = bullets[i].vx * inv, dy = bullets[i].vy * inv;
        float px = -dy, py = dx;
        float x = bullets[i].x, y = bullets[i].y;
        put_vert(buf, slot,     x + dx * BULLET_LEN, y + dy * BULLET_LEN, ARENA_Z, 0xFFFFC8FF);
        put_vert(buf, slot + 1, x + px * BULLET_W,   y + py * BULLET_W,   ARENA_Z, 0xFFB43CFF);
        put_vert(buf, slot + 2, x - dx * BULLET_LEN, y - dy * BULLET_LEN, ARENA_Z, 0xB4500AFF);
        put_vert(buf, slot + 3, x - px * BULLET_W,   y - py * BULLET_W,   ARENA_Z, 0xFFB43CFF);
    }

    /* --- Shards: spinning triangles on the class-delta hue; flicker
     * when about to expire. --- */
    float hue = palette_base_hue(time) + 0.28f;
    for (int i = 0; i < MAX_SHARDS; i++) {
        int slot = MAX_BULLETS * 4 + i * 4;
        shard_t *s = &shards[i];
        if (!s->alive || (s->life < 2.f && ((int)(time * 12.f) & 1))) {
            collapse_slot4(buf, slot);
            continue;
        }
        uint32_t col = palette_hsv_rgba(hue, 0.85f, 0.95f);
        for (int k = 0; k < 3; k++) {
            float a = s->spin + (float)k * 2.0944f;
            put_vert(buf, slot + k,
                     s->x + fm_cosf(a) * SHARD_R,
                     s->y + fm_sinf(a) * SHARD_R, ARENA_Z, col);
        }
        put_vert(buf, slot + 3, 0.f, 0.f, 0.f, 0);
    }

    /* --- Enemy matrices --- */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        float grow = 1.f - e->spawn_t * 1.25f;   /* pop in over the grace */
        if (grow < 0.15f) grow = 0.15f;
        if (grow > 1.f)   grow = 1.f;
        float base = (e->gen == 0 ? 1.f : 0.6f) * grow;
        float sc[3] = { base * e->jit[0], base * e->jit[1], base * e->jit[2] };
        float half = e->spin * 0.5f;
        float sh = fm_sinf(half);
        float q[4] = { e->axis[0] * sh, e->axis[1] * sh, e->axis[2] * sh,
                       fm_cosf(half) };
        float pos[3] = { e->x, e->y, ARENA_Z };
        t3d_mat4fp_from_srt(&enemy_mats[fi][i], sc, q, pos);
    }
}

void render_entities_draw(int fi, float time) {
    /* Enemies: real depth so rotating tetrahedra self-occlude correctly
     * (Z was cleared after the background layers, so they only ever
     * contest each other). White facet-shaded verts x prim color, so
     * the class hue rides the palette rotation (GDD 5.3). */
    rdpq_mode_zbuf(true, true);
    rdpq_mode_combiner(RDPQ_COMBINER1((SHADE, 0, PRIM, 0), (0, 0, 0, SHADE)));
    float hue = palette_base_hue(time) + WANDERER_HUE;
    uint32_t c0 = palette_hsv_rgba(hue, 0.90f, 1.0f);
    uint32_t c1 = palette_hsv_rgba(hue + 0.06f, 0.75f, 1.0f);

    t3d_matrix_push_pos(1);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        t3d_matrix_set(&enemy_mats[fi][i], false);
        uint32_t c = (e->gen == 0) ? c0 : c1;
        rdpq_set_prim_color(RGBA32((c >> 24) & 0xFF, (c >> 16) & 0xFF,
                                   (c >> 8) & 0xFF, 0xFF));
        t3d_vert_load(tetra_verts + (i % TETRA_VARIANTS) * TETRA_PACKED_COUNT,
                      0, TETRA_VERTS);
        mesh_tetra_draw();
        t3d_tri_sync();
    }
    t3d_matrix_pop(1);

    /* Bullets + shards: readability-critical, always painted on top —
     * no Z test or write (GDD 1.1 #3). Leaves Z off for the ship pass. */
    rdpq_mode_zbuf(false, false);
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rspq_block_run(proj_dpl[fi]);
}
