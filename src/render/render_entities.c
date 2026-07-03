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

#define BULLET_LEN       7.f
#define BULLET_W         2.5f
#define EBOLT_LEN        5.f
#define SHARD_R          5.f

/* Class hue deltas off the wheel walk (GDD 5.3): at base hue 0 (red)
 * these land on the GDD 3.1 color families. */
static const float SPECIES_HUE[SPECIES_COUNT] = {
    0.50f,   /* Wanderer: cyan    */
    0.83f,   /* Seeker:   magenta */
    0.14f,   /* Swarmer:  yellow  */
    0.07f,   /* Turret:   orange  */
    0.00f,   /* Pulsar:   red     */
    0.33f,   /* Snake:    green   */
};

/* Bullets: 4 verts (2 packed) per elongated diamond; enemy bolts the
 * same shape; shards: 3 verts + pad (2 packed). Inactive slots collapse
 * to the origin, which the near plane clips away. */
#define BULLET_PACKED    (MAX_BULLETS * 2)
#define EBOLT_PACKED     (MAX_EBULLETS * 2)
#define SHARD_PACKED     (MAX_SHARDS * 2)
#define PROJ_PACKED      (BULLET_PACKED + EBOLT_PACKED + SHARD_PACKED)

/* Species mesh atlas: one static buffer, offsets in packed entries. */
#define OFF_TETRA(v)     ((v) * TETRA_PACKED_COUNT)
#define OFF_SEEKER       (TETRA_VARIANTS * TETRA_PACKED_COUNT)
#define OFF_SWARMER      (OFF_SEEKER + SEEKER_PACKED_COUNT)
#define OFF_TURRET       (OFF_SWARMER + SWARMER_PACKED_COUNT)
#define OFF_PULSAR       (OFF_TURRET + TURRET_PACKED_COUNT)
#define ATLAS_PACKED     (OFF_PULSAR + PULSAR_PACKED_COUNT)

static T3DVertPacked *atlas_verts;
static T3DMat4FP     *enemy_mats[2];
static T3DVertPacked *proj_verts[2];
static rspq_block_t  *proj_dpl[2];
static T3DMat4FP     *ident_mat;

static rspq_block_t *record_proj_dpl(T3DVertPacked *verts) {
    rspq_block_t *dpl;
    rspq_block_begin();
        t3d_matrix_push(ident_mat);
        /* Player bullets + enemy bolts: quads, chunks of 16 (64 verts). */
        for (int base = 0; base < MAX_BULLETS + MAX_EBULLETS; base += 16) {
            int n = MAX_BULLETS + MAX_EBULLETS - base; if (n > 16) n = 16;
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
            t3d_vert_load(verts + (BULLET_PACKED + EBOLT_PACKED) + base * 2,
                          0, n * 4);
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

    atlas_verts = malloc_uncached(sizeof(T3DVertPacked) * ATLAS_PACKED);
    for (int v = 0; v < TETRA_VARIANTS; v++)
        mesh_gen_tetra(atlas_verts + OFF_TETRA(v), TETRA_RADIUS,
                       0xBEEF0001u + (uint32_t)v);
    mesh_gen_seeker (atlas_verts + OFF_SEEKER,  0x5EEC0001u);
    mesh_gen_swarmer(atlas_verts + OFF_SWARMER, 0x50AC0001u);
    mesh_gen_turret (atlas_verts + OFF_TURRET,  0x70AE0001u);
    mesh_gen_pulsar (atlas_verts + OFF_PULSAR,  0xB00C0001u);

    for (int i = 0; i < 2; i++) {
        enemy_mats[i] = malloc_uncached(sizeof(T3DMat4FP) * MAX_ENEMIES);
        for (int e = 0; e < MAX_ENEMIES; e++)
            t3d_mat4fp_identity(&enemy_mats[i][e]);

        proj_verts[i] = malloc_uncached(sizeof(T3DVertPacked) * PROJ_PACKED);
        for (int p = 0; p < PROJ_PACKED; p++)
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

static void put_bolt_quad(T3DVertPacked *buf, int slot, const bullet_t *b,
                          float len, float speed,
                          uint32_t head, uint32_t side, uint32_t tail) {
    float inv = 1.f / speed;
    float dx = b->vx * inv, dy = b->vy * inv;
    float px = -dy, py = dx;
    put_vert(buf, slot,     b->x + dx * len,      b->y + dy * len,      ARENA_Z, head);
    put_vert(buf, slot + 1, b->x + px * BULLET_W, b->y + py * BULLET_W, ARENA_Z, side);
    put_vert(buf, slot + 2, b->x - dx * len,      b->y - dy * len,      ARENA_Z, tail);
    put_vert(buf, slot + 3, b->x - px * BULLET_W, b->y - py * BULLET_W, ARENA_Z, side);
}

/* Compose a Z-rotation with the tumble spin for species that face their
 * heading; plain axis-angle for the tumblers. */
static void quat_for(const enemy_t *e, float q[4]) {
    switch (e->species) {
    case SP_SEEKER:
    case SP_SWARMER: {
        float ha = e->heading - 1.5707963f;   /* mesh nose = +Y */
        q[0] = 0.f; q[1] = 0.f;
        q[2] = fm_sinf(ha * 0.5f); q[3] = fm_cosf(ha * 0.5f);
        break;
    }
    case SP_TURRET: {
        q[0] = 0.f; q[1] = 0.f;
        q[2] = fm_sinf(e->spin * 0.5f); q[3] = fm_cosf(e->spin * 0.5f);
        break;
    }
    default: {   /* Wanderer, Pulsar, Snake: tumble */
        float half = e->spin * 0.5f;
        float sh = fm_sinf(half);
        q[0] = e->axis[0] * sh; q[1] = e->axis[1] * sh;
        q[2] = e->axis[2] * sh; q[3] = fm_cosf(half);
        break;
    }
    }
}

void render_entities_build(int fi, float time) {
    T3DVertPacked *buf = proj_verts[fi];

    /* --- Player bullets: warm & fixed-color for readability. --- */
    for (int i = 0; i < MAX_BULLETS; i++) {
        int slot = i * 4;
        if (!bullets[i].alive) { collapse_slot4(buf, slot); continue; }
        put_bolt_quad(buf, slot, &bullets[i], BULLET_LEN, 500.f,
                      0xFFFFC8FF, 0xFFB43CFF, 0xB4500AFF);
    }

    /* --- Enemy bolts: hot pink-red, unmistakably hostile. --- */
    for (int i = 0; i < MAX_EBULLETS; i++) {
        int slot = (MAX_BULLETS + i) * 4;
        if (!ebullets[i].alive) { collapse_slot4(buf, slot); continue; }
        put_bolt_quad(buf, slot, &ebullets[i], EBOLT_LEN, 95.f,
                      0xFFFFFFFF, 0xFF3264FF, 0xA00A28FF);
    }

    /* --- Shards: spinning triangles on the class-delta hue; flicker
     * when about to expire. --- */
    float hue = palette_base_hue(time) + 0.28f;
    for (int i = 0; i < MAX_SHARDS; i++) {
        int slot = (MAX_BULLETS + MAX_EBULLETS) * 4 + i * 4;
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

        float base = grow;
        if (e->species == SP_WANDERER && e->gen == 1) base *= 0.6f;
        if (e->species == SP_SNAKE && e->lead >= 0)   base *= 0.75f;
        if (e->species == SP_PULSAR) {
            /* Swell toward the burst, throbbing faster near the end
             * (GDD 3.1: grows, then bursts). */
            float t = 1.f - e->timer / PULSAR_FUSE;
            base *= 0.65f + t * 0.85f
                  + fm_sinf(time * (4.f + t * 14.f)) * 0.10f * t;
        }

        float sc[3] = { base * e->jit[0], base * e->jit[1], base * e->jit[2] };
        float q[4];
        quat_for(e, q);
        float pos[3] = { e->x, e->y, ARENA_Z };
        t3d_mat4fp_from_srt(&enemy_mats[fi][i], sc, q, pos);
    }
}

void render_entities_draw(int fi, float time) {
    /* Enemies: real depth so rotating meshes self-occlude correctly
     * (Z was cleared after the background layers, so they only ever
     * contest each other). White facet-shaded verts x prim color, so
     * the class hue rides the palette rotation (GDD 5.3). */
    rdpq_mode_zbuf(true, true);
    rdpq_mode_combiner(RDPQ_COMBINER1((SHADE, 0, PRIM, 0), (0, 0, 0, SHADE)));

    float base_hue = palette_base_hue(time);
    uint32_t class_col[SPECIES_COUNT];
    for (int s = 0; s < SPECIES_COUNT; s++)
        class_col[s] = palette_hsv_rgba(base_hue + SPECIES_HUE[s], 0.90f, 1.0f);

    t3d_matrix_push_pos(1);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        t3d_matrix_set(&enemy_mats[fi][i], false);

        uint32_t c = class_col[e->species];
        if (e->species == SP_WANDERER && e->gen == 1)
            c = palette_hsv_rgba(base_hue + SPECIES_HUE[SP_WANDERER] + 0.06f,
                                 0.75f, 1.0f);
        /* Armored snake segments read darker; the tail glows. */
        if (e->species == SP_SNAKE)
            c = enemy_vulnerable(i)
              ? palette_hsv_rgba(base_hue + SPECIES_HUE[SP_SNAKE], 0.85f, 1.0f)
              : palette_hsv_rgba(base_hue + SPECIES_HUE[SP_SNAKE], 0.95f, 0.55f);

        rdpq_set_prim_color(RGBA32((c >> 24) & 0xFF, (c >> 16) & 0xFF,
                                   (c >> 8) & 0xFF, 0xFF));

        switch (e->species) {
        case SP_SEEKER:
            t3d_vert_load(atlas_verts + OFF_SEEKER, 0, SEEKER_VERTS);
            mesh_seeker_draw();
            break;
        case SP_SWARMER:
            t3d_vert_load(atlas_verts + OFF_SWARMER, 0, SWARMER_VERTS);
            mesh_swarmer_draw();
            break;
        case SP_TURRET:
            t3d_vert_load(atlas_verts + OFF_TURRET, 0, TURRET_VERTS);
            mesh_turret_draw();
            break;
        case SP_PULSAR:
            t3d_vert_load(atlas_verts + OFF_PULSAR, 0, PULSAR_VERTS);
            mesh_pulsar_draw();
            break;
        default:   /* Wanderer + Snake segments: jittered tetra variants */
            t3d_vert_load(atlas_verts + OFF_TETRA(i % TETRA_VARIANTS),
                          0, TETRA_VERTS);
            mesh_tetra_draw();
            break;
        }
        t3d_tri_sync();
    }
    t3d_matrix_pop(1);

    /* Bullets, bolts + shards: readability-critical, always painted on
     * top — no Z test or write (GDD 1.1 #3). Leaves Z off for the ship. */
    rdpq_mode_zbuf(false, false);
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rspq_block_run(proj_dpl[fi]);
}
