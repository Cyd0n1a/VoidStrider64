#include "director.h"
#include "arena.h"
#include "enemies.h"
#include "../meta/scoring.h"
#include <math.h>

#define SPAWN_MIN_DIST 100.f   /* never spawn in the player's face */

static float    elapsed;
static float    spawn_timer;
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

void director_init(uint32_t seed) {
    rng_state   = seed ? seed : 1;
    elapsed     = 0.f;
    spawn_timer = 1.2f;
}

static void spawn_edge(float px, float py) {
    /* Pick an edge point far enough from the player; a few retries are
     * plenty on an arena this size. */
    for (int try = 0; try < 6; try++) {
        int   edge = (int)(rng() & 3);
        float x, y;
        switch (edge) {
            case 0:  x = -ARENA_HALF_W + 14.f; y = frange(-ARENA_HALF_H, ARENA_HALF_H); break;
            case 1:  x =  ARENA_HALF_W - 14.f; y = frange(-ARENA_HALF_H, ARENA_HALF_H); break;
            case 2:  y = -ARENA_HALF_H + 14.f; x = frange(-ARENA_HALF_W, ARENA_HALF_W); break;
            default: y =  ARENA_HALF_H - 14.f; x = frange(-ARENA_HALF_W, ARENA_HALF_W); break;
        }
        float dx = x - px, dy = y - py;
        if (dx * dx + dy * dy < SPAWN_MIN_DIST * SPAWN_MIN_DIST) continue;
        enemies_spawn(x, y, 0);
        return;
    }
}

void director_update(float dt, float px, float py) {
    elapsed += dt;

    /* Difficulty scales continuously with elapsed time (GDD 3.6):
     * spawn interval 2.4s -> 0.85s over ~1 minute, alive cap rises. */
    float interval = 2.4f - elapsed * 0.026f;
    if (interval < 0.85f) interval = 0.85f;

    int cap = 6 + (int)(elapsed / 12.f);
    if (cap > 18) cap = 18;    /* leave pool room for split children */

    spawn_timer -= dt;
    if (spawn_timer <= 0.f) {
        spawn_timer += interval;
        if (enemies_count() < cap)
            spawn_edge(px, py);
    }
}

float director_intensity(void) {
    float i = (float)enemies_count() / 14.f + scoring_kill_pulse();
    return i > 1.f ? 1.f : i;
}
