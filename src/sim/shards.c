#include "shards.h"
#include "arena.h"
#include "../meta/scoring.h"
#include "../audio/synth.h"
#include <math.h>
#include <stdint.h>

#define SHARD_LIFE     8.f
#define MAGNET_RADIUS  60.f
#define MAGNET_ACCEL   900.f
#define COLLECT_RADIUS 15.f
#define DRAG           2.2f

shard_t shards[MAX_SHARDS];

static uint32_t rng_state = 0xD1CEu;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static float frange(float a, float b) {
    return a + ((float)(rng() >> 8) / 16777216.f) * (b - a);
}

void shards_init(void)  { shards_clear(); }
void shards_clear(void) {
    for (int i = 0; i < MAX_SHARDS; i++) shards[i].alive = false;
}

void shards_burst(float x, float y, int count) {
    for (int c = 0; c < count; c++) {
        for (int i = 0; i < MAX_SHARDS; i++) {
            if (shards[i].alive) continue;
            float ang = frange(0.f, 6.2831853f);
            float spd = frange(50.f, 130.f);
            shards[i] = (shard_t){
                .alive = true,
                .x = x, .y = y,
                .vx = cosf(ang) * spd, .vy = sinf(ang) * spd,
                .life = SHARD_LIFE,
                .spin = frange(0.f, 6.2831853f),
            };
            break;
        }
    }
}

void shards_update(float dt, float px, float py) {
    for (int i = 0; i < MAX_SHARDS; i++) {
        shard_t *s = &shards[i];
        if (!s->alive) continue;

        s->life -= dt;
        if (s->life <= 0.f) { s->alive = false; continue; }

        float dx = px - s->x, dy = py - s->y;
        float d2 = dx * dx + dy * dy;

        if (d2 < COLLECT_RADIUS * COLLECT_RADIUS) {
            s->alive = false;
            synth_shard(scoring_shard());
            continue;
        }

        /* Magnet pull once the ship is close (GW-style vacuum). */
        if (d2 < MAGNET_RADIUS * MAGNET_RADIUS && d2 > 1.f) {
            float inv = 1.f / sqrtf(d2);
            s->vx += dx * inv * MAGNET_ACCEL * dt;
            s->vy += dy * inv * MAGNET_ACCEL * dt;
        }

        float drag = 1.f - DRAG * dt;
        if (drag < 0.f) drag = 0.f;
        s->vx *= drag;
        s->vy *= drag;

        s->x += s->vx * dt;
        s->y += s->vy * dt;
        s->spin += 4.f * dt;

        if (s->x < -ARENA_HALF_W) s->x = -ARENA_HALF_W;
        if (s->x >  ARENA_HALF_W) s->x =  ARENA_HALF_W;
        if (s->y < -ARENA_HALF_H) s->y = -ARENA_HALF_H;
        if (s->y >  ARENA_HALF_H) s->y =  ARENA_HALF_H;
    }
}
