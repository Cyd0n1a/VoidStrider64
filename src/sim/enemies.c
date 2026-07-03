#include "enemies.h"
#include "arena.h"
#include "../meta/scoring.h"
#include "../audio/synth.h"
#include "../gen/grid_sim.h"
#include <libdragon.h>
#include <math.h>

#define SPEED_GEN0   48.f
#define SPEED_GEN1   72.f
#define SPAWN_GRACE  0.8f   /* fade-in seconds before a spawn can collide */

enemy_t enemies[MAX_ENEMIES];

/* Behavior RNG: separate stream from cosmetics (GDD 8.3). */
static uint32_t rng_state;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static float frange(float a, float b) {
    return a + ((float)(rng() >> 8) / 16777216.f) * (b - a);
}

float enemy_radius(const enemy_t *e) {
    return e->gen == 0 ? 13.f : 8.f;
}

void enemies_init(uint32_t seed) {
    rng_state = seed ? seed : 1;
    enemies_clear();
}

void enemies_clear(void) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        enemies[i].alive = false;
}

int enemies_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive) n++;
    return n;
}

int enemies_spawn(float x, float y, int gen) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) continue;
        enemy_t *e = &enemies[i];
        e->alive   = true;
        e->gen     = gen;
        e->x = x; e->y = y;
        e->heading      = frange(0.f, 6.2831853f);
        e->wander_phase = frange(0.f, 6.2831853f);
        e->wander_spd   = frange(0.8f, 1.6f);
        e->spin         = frange(0.f, 6.2831853f);
        e->spin_spd     = frange(1.5f, 3.5f) * ((rng() & 1) ? 1.f : -1.f);

        float ax = frange(-1.f, 1.f), ay = frange(-1.f, 1.f), az = frange(0.4f, 1.f);
        float inv = 1.f / sqrtf(ax * ax + ay * ay + az * az);
        e->axis[0] = ax * inv; e->axis[1] = ay * inv; e->axis[2] = az * inv;

        for (int j = 0; j < 3; j++)
            e->jit[j] = frange(0.88f, 1.12f);   /* per-spawn jitter (GDD 3.1) */
        e->spawn_t = SPAWN_GRACE;
        return i;
    }
    return -1;
}

void enemies_update(float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &enemies[i];
        if (!e->alive) continue;

        if (e->spawn_t > 0.f) e->spawn_t -= dt;

        /* Noise-driven drift: heading wobbles with a per-enemy LFO
         * (GDD 3.1 Wanderer). */
        e->wander_phase += e->wander_spd * dt;
        e->heading += fm_sinf(e->wander_phase) * 1.4f * dt;

        float spd = (e->gen == 0) ? SPEED_GEN0 : SPEED_GEN1;
        e->x += fm_cosf(e->heading) * spd * dt;
        e->y += fm_sinf(e->heading) * spd * dt;

        /* Reflect off arena walls. */
        float rad = enemy_radius(e);
        if (e->x < -ARENA_HALF_W + rad) { e->x = -ARENA_HALF_W + rad; e->heading = 3.1415927f - e->heading; }
        if (e->x >  ARENA_HALF_W - rad) { e->x =  ARENA_HALF_W - rad; e->heading = 3.1415927f - e->heading; }
        if (e->y < -ARENA_HALF_H + rad) { e->y = -ARENA_HALF_H + rad; e->heading = -e->heading; }
        if (e->y >  ARENA_HALF_H - rad) { e->y =  ARENA_HALF_H - rad; e->heading = -e->heading; }

        e->spin += e->spin_spd * dt;
    }
}

void enemies_kill(int idx, bool allow_split) {
    enemy_t *e = &enemies[idx];
    int   gen = e->gen;
    float x = e->x, y = e->y;
    e->alive = false;

    scoring_kill(gen);
    synth_enemy_die(gen);
    grid_impulse(x, y, gen == 0 ? 42.f : 24.f, gen == 0 ? 70.f : 45.f);

    /* Wanderers split into two smaller ones (GDD 3.1). Children get a
     * grace period so they don't instantly eat the killing stream.
     * Smart bombs vaporize outright — no splits (GDD 3.5). */
    if (gen == 0 && allow_split) {
        for (int c = 0; c < 2; c++) {
            int ci = enemies_spawn(x + frange(-6.f, 6.f), y + frange(-6.f, 6.f), 1);
            if (ci < 0) break;
            enemies[ci].spawn_t = 0.35f;
        }
    }
}
