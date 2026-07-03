#include "enemies.h"
#include "arena.h"
#include "projectiles.h"
#include "../meta/scoring.h"
#include "../audio/synth.h"
#include "../gen/grid_sim.h"
#include <libdragon.h>
#include <math.h>

#define SPAWN_GRACE     0.8f
#define SNAKE_SPACING   17.f
#define TURRET_PERIOD   2.3f
#define TURRET_WARMUP   1.2f
#define EBOLT_SPEED     95.f

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

/* ---- Per-species tuning tables ---- */
static const float SPEED[SPECIES_COUNT]   = { 48.f, 85.f, 95.f, 0.f, 0.f, 78.f };
static const float RADIUS[SPECIES_COUNT]  = { 13.f, 11.f, 6.f, 14.f, 12.f, 9.f };
static const int   POINTS[SPECIES_COUNT]  = { 30, 50, 10, 40, 60, 20 };
static const int   SHARDS[SPECIES_COUNT]  = { 3, 3, 1, 4, 5, 2 };
static const float SFXSIZE[SPECIES_COUNT] = { 1.f, 0.8f, 0.4f, 1.1f, 1.3f, 0.7f };

#define SNAKE_HEAD_POINTS 150
#define SNAKE_HEAD_SHARDS 8

float enemy_radius(const enemy_t *e) {
    if (e->species == SP_WANDERER && e->gen == 1) return 8.f;
    return RADIUS[e->species];
}

int enemy_shard_count(const enemy_t *e) {
    if (e->species == SP_WANDERER && e->gen == 1) return 2;
    if (e->species == SP_SNAKE && e->lead < 0)    return SNAKE_HEAD_SHARDS;
    return SHARDS[e->species];
}

bool enemy_vulnerable(int idx) {
    if (enemies[idx].species != SP_SNAKE) return true;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive && enemies[i].species == SP_SNAKE &&
            enemies[i].lead == idx)
            return false;   /* something still follows it: armored */
    return true;
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

int enemies_count_species(species_t sp) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive && enemies[i].species == sp) n++;
    return n;
}

int enemies_spawn(float x, float y, species_t sp, int gen) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) continue;
        enemy_t *e = &enemies[i];
        e->alive    = true;
        e->species  = sp;
        e->gen      = gen;
        e->x = x; e->y = y;
        e->heading      = frange(0.f, 6.2831853f);
        e->wander_phase = frange(0.f, 6.2831853f);
        e->wander_spd   = frange(0.8f, 1.6f);
        e->spin         = frange(0.f, 6.2831853f);
        e->spin_spd     = frange(1.5f, 3.5f) * ((rng() & 1) ? 1.f : -1.f);
        if (sp == SP_TURRET) e->spin_spd *= 0.25f;   /* stately rotation */

        float ax = frange(-1.f, 1.f), ay = frange(-1.f, 1.f), az = frange(0.4f, 1.f);
        float inv = 1.f / sqrtf(ax * ax + ay * ay + az * az);
        e->axis[0] = ax * inv; e->axis[1] = ay * inv; e->axis[2] = az * inv;

        for (int j = 0; j < 3; j++)
            e->jit[j] = frange(0.88f, 1.12f);
        e->spawn_t = SPAWN_GRACE;
        e->timer   = (sp == SP_TURRET) ? TURRET_WARMUP
                   : (sp == SP_PULSAR) ? PULSAR_FUSE : 0.f;
        e->lead    = -1;
        return i;
    }
    return -1;
}

void enemies_spawn_snake(float x, float y, int segments) {
    int prev = enemies_spawn(x, y, SP_SNAKE, 0);
    if (prev < 0) return;
    for (int s = 1; s < segments; s++) {
        int idx = enemies_spawn(x - (float)s * SNAKE_SPACING * 0.5f, y,
                                SP_SNAKE, 0);
        if (idx < 0) return;
        enemies[idx].lead = prev;
        prev = idx;
    }
}

static void bounce_walls(enemy_t *e) {
    float rad = enemy_radius(e);
    if (e->x < -ARENA_HALF_W + rad) { e->x = -ARENA_HALF_W + rad; e->heading = 3.1415927f - e->heading; }
    if (e->x >  ARENA_HALF_W - rad) { e->x =  ARENA_HALF_W - rad; e->heading = 3.1415927f - e->heading; }
    if (e->y < -ARENA_HALF_H + rad) { e->y = -ARENA_HALF_H + rad; e->heading = -e->heading; }
    if (e->y >  ARENA_HALF_H - rad) { e->y =  ARENA_HALF_H - rad; e->heading = -e->heading; }
}

static void turn_toward(enemy_t *e, float target, float rate, float dt) {
    float diff = target - e->heading;
    while (diff >  3.1415927f) diff -= 6.2831853f;
    while (diff < -3.1415927f) diff += 6.2831853f;
    float tb = dt * rate;
    if (tb > 1.f) tb = 1.f;
    e->heading += diff * tb;
}

static void pulsar_burst(enemy_t *e) {
    /* Ring of eight bolts (GDD 3.1). The pulsar spends itself: no score,
     * the player was warned for five full seconds. */
    for (int k = 0; k < 8; k++) {
        float a = 6.2831853f * (float)k / 8.f + e->spin;
        ebullet_spawn(e->x, e->y,
                      fm_cosf(a) * EBOLT_SPEED, fm_sinf(a) * EBOLT_SPEED);
    }
    e->alive = false;
    synth_enemy_die(1.3f);
    synth_ebolt();
    grid_impulse(e->x, e->y, 55.f, 90.f);
}

void enemies_update(float dt, float px, float py, float pvx, float pvy) {
    /* Swarmer flock centroid (boids-lite cohesion, GDD 3.1). */
    float cx = 0.f, cy = 0.f;
    int   nsw = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive && enemies[i].species == SP_SWARMER) {
            cx += enemies[i].x; cy += enemies[i].y; nsw++;
        }
    }
    if (nsw > 0) { cx /= (float)nsw; cy /= (float)nsw; }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemy_t *e = &enemies[i];
        if (!e->alive) continue;

        if (e->spawn_t > 0.f) e->spawn_t -= dt;
        e->spin += e->spin_spd * dt;

        float spd = SPEED[e->species];

        switch (e->species) {
        case SP_WANDERER:
            e->wander_phase += e->wander_spd * dt;
            e->heading += fm_sinf(e->wander_phase) * 1.4f * dt;
            if (e->gen == 1) spd = 72.f;
            e->x += fm_cosf(e->heading) * spd * dt;
            e->y += fm_sinf(e->heading) * spd * dt;
            bounce_walls(e);
            break;

        case SP_SEEKER: {
            /* Pursue with lead prediction (GDD 3.1): aim where the
             * player will be, capped so it can't over-anticipate. */
            float dx = px - e->x, dy = py - e->y;
            float lead = sqrtf(dx * dx + dy * dy) / spd;
            if (lead > 0.8f) lead = 0.8f;
            turn_toward(e, atan2f(py + pvy * lead - e->y,
                                  px + pvx * lead - e->x), 3.2f, dt);
            e->x += fm_cosf(e->heading) * spd * dt;
            e->y += fm_sinf(e->heading) * spd * dt;
            bounce_walls(e);
            break;
        }

        case SP_SWARMER: {
            /* Boids-lite: pursue player + cohere to flock + separate
             * from packmates (GDD 3.1). */
            float dx = px - e->x, dy = py - e->y;
            float dp = sqrtf(dx * dx + dy * dy);
            if (dp < 1.f) dp = 1.f;
            float wx = dx / dp, wy = dy / dp;
            if (nsw > 1) {
                wx += (cx - e->x) * 0.006f;
                wy += (cy - e->y) * 0.006f;
                for (int o = 0; o < MAX_ENEMIES; o++) {
                    if (o == i || !enemies[o].alive ||
                        enemies[o].species != SP_SWARMER) continue;
                    float sx = e->x - enemies[o].x, sy = e->y - enemies[o].y;
                    float d2 = sx * sx + sy * sy;
                    if (d2 < 1.f) d2 = 1.f;
                    if (d2 < 26.f * 26.f) {
                        wx += sx / d2 * 14.f;
                        wy += sy / d2 * 14.f;
                    }
                }
            }
            turn_toward(e, atan2f(wy, wx), 4.f, dt);
            e->x += fm_cosf(e->heading) * spd * dt;
            e->y += fm_sinf(e->heading) * spd * dt;
            bounce_walls(e);
            break;
        }

        case SP_TURRET:
            /* Static; fires slow aimed bolts (GDD 3.1). */
            if (e->spawn_t <= 0.f) {
                e->timer -= dt;
                if (e->timer <= 0.f) {
                    e->timer += TURRET_PERIOD;
                    float dx = px - e->x, dy = py - e->y;
                    float d = sqrtf(dx * dx + dy * dy);
                    if (d > 1.f) {
                        ebullet_spawn(e->x, e->y,
                                      dx / d * EBOLT_SPEED, dy / d * EBOLT_SPEED);
                        synth_ebolt();
                    }
                }
            }
            break;

        case SP_PULSAR:
            /* Grows toward the burst; render scales off timer. */
            e->timer -= dt;
            if (e->timer <= 0.f) pulsar_burst(e);
            break;

        case SP_SNAKE:
            if (e->lead < 0) {
                /* Head: strong sinuous path (GDD 3.1). */
                e->wander_phase += 2.0f * dt;
                e->heading += fm_sinf(e->wander_phase) * 2.4f * dt;
                e->x += fm_cosf(e->heading) * spd * dt;
                e->y += fm_sinf(e->heading) * spd * dt;
                bounce_walls(e);
            } else if (enemies[e->lead].alive) {
                /* Body: follow-the-leader keeping fixed spacing. */
                float dx = enemies[e->lead].x - e->x;
                float dy = enemies[e->lead].y - e->y;
                float d  = sqrtf(dx * dx + dy * dy);
                if (d > SNAKE_SPACING) {
                    float pull = (d - SNAKE_SPACING) * 10.f;
                    if (pull > 150.f) pull = 150.f;
                    e->x += dx / d * pull * dt;
                    e->y += dy / d * pull * dt;
                    e->heading = atan2f(dy, dx);
                }
            }
            break;

        default: break;
        }
    }
}

void enemies_kill(int idx, bool allow_split) {
    enemy_t *e = &enemies[idx];
    species_t sp = e->species;
    int   gen = e->gen;
    float x = e->x, y = e->y;
    bool  snake_head = (sp == SP_SNAKE && e->lead < 0);
    e->alive = false;

    int points = POINTS[sp];
    if (sp == SP_WANDERER && gen == 1) points = 15;
    if (snake_head)                    points = SNAKE_HEAD_POINTS;
    scoring_kill(points);

    synth_enemy_die(snake_head ? 1.2f : SFXSIZE[sp]);
    float boom = enemy_radius(e) * 3.2f;
    grid_impulse(x, y, boom, boom * 1.7f);

    /* Wanderers split into two smaller ones (GDD 3.1); bombs vaporize
     * outright (GDD 3.5). Children get a grace period so they don't
     * instantly eat the killing stream. */
    if (sp == SP_WANDERER && gen == 0 && allow_split) {
        for (int c = 0; c < 2; c++) {
            int ci = enemies_spawn(x + frange(-6.f, 6.f), y + frange(-6.f, 6.f),
                                   SP_WANDERER, 1);
            if (ci < 0) break;
            enemies[ci].spawn_t = 0.35f;
        }
    }
}
