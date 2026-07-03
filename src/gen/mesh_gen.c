#include "mesh_gen.h"
#include <t3d/t3d.h>
#include <string.h>

/* Local xorshift so ship generation doesn't disturb the tunnel's RNG
 * stream (GDD 8.3: separate cosmetic/difficulty streams). */
static uint32_t ship_rng_state;
static uint32_t srng(void) {
    ship_rng_state ^= ship_rng_state << 13;
    ship_rng_state ^= ship_rng_state >> 17;
    ship_rng_state ^= ship_rng_state << 5;
    return ship_rng_state;
}
static float sfrange(float a, float b) {
    return a + ((float)(srng() >> 8) / 16777216.f) * (b - a);
}

void mesh_gen_ship(T3DVertPacked *out, uint32_t seed) {
    ship_rng_state = seed ? seed : 1;

    /* Arrowhead with a vertical keel diamond; ±10% seeded jitter. */
    float nose  = 16.f * sfrange(0.9f, 1.1f);
    float span  = 11.f * sfrange(0.9f, 1.1f);
    float sweep =  9.f * sfrange(0.9f, 1.1f);
    float keel  =  5.f * sfrange(0.9f, 1.1f);

    /* Ship stays white/bright regardless of the drifting palette so the
     * player always reads instantly (GDD 1.1 #3, 5.2). */
    const uint32_t COL_CORE = 0xFFFFFFFF;
    const uint32_t COL_WING = 0xB4F0FFFF;
    const uint32_t COL_KEEL = 0x8CB4DCFF;

    float vx[SHIP_HULL_VERTS] = {   0.f, -span,  span,   0.f,   0.f,   0.f };
    float vy[SHIP_HULL_VERTS] = {  nose, -sweep, -sweep, -5.f, -2.f,  -2.f };
    float vz[SHIP_HULL_VERTS] = {   0.f,   0.f,   0.f,   0.f,  keel, -keel };
    uint32_t vc[SHIP_HULL_VERTS] = { COL_CORE, COL_WING, COL_WING,
                                     COL_WING, COL_KEEL, COL_KEEL };

    memset(out, 0, sizeof(T3DVertPacked) * SHIP_PACKED_COUNT);
    for (int i = 0; i < SHIP_HULL_VERTS; i += 2) {
        out[i / 2] = (T3DVertPacked){
            .posA = { (int16_t)vx[i],   (int16_t)vy[i],   (int16_t)vz[i]   },
            .normA = 0, .rgbaA = vc[i],     .stA = {0, 0},
            .posB = { (int16_t)vx[i+1], (int16_t)vy[i+1], (int16_t)vz[i+1] },
            .normB = 0, .rgbaB = vc[i + 1], .stB = {0, 0},
        };
    }
    /* Thruster slots (verts 6..8) start collapsed; renderer animates them. */
}

void mesh_ship_draw_hull(void) {
    /* Vert slots: 0=nose 1=wingL 2=wingR 3=tail 4=keelTop 5=keelBot.
     * Double-sided (no cull flags), so winding is cosmetic. */
    t3d_tri_draw(0, 1, 4);
    t3d_tri_draw(0, 4, 2);
    t3d_tri_draw(0, 5, 1);
    t3d_tri_draw(0, 2, 5);
    t3d_tri_draw(1, 3, 4);
    t3d_tri_draw(4, 3, 2);
    t3d_tri_draw(1, 5, 3);
    t3d_tri_draw(3, 5, 2);
}
