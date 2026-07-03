#include "scoring.h"

#define SHARDS_PER_MULT   5
#define MULT_MAX          10
#define DECAY_GRACE       4.f    /* seconds without a pickup before decay */
#define DECAY_INTERVAL    0.5f   /* then one shard's worth drains per tick */

static uint32_t score;
static int      shards_banked;   /* lifetime-of-streak shard count */
static int      chain;           /* consecutive pickups, resets with mult drop */
static float    since_pickup;
static float    decay_timer;
static float    kill_pulse;

void scoring_init(void) {
    score = 0;
    shards_banked = 0;
    chain = 0;
    since_pickup = 0.f;
    decay_timer = 0.f;
    kill_pulse = 0.f;
}

int scoring_mult(void) {
    int m = 1 + shards_banked / SHARDS_PER_MULT;
    return m > MULT_MAX ? MULT_MAX : m;
}

void scoring_kill(int base_points) {
    /* Per-species base points live with the species defs (enemies.c). */
    score += (uint32_t)base_points * (uint32_t)scoring_mult();
    kill_pulse += 0.22f;
    if (kill_pulse > 1.f) kill_pulse = 1.f;
}

int scoring_shard(void) {
    shards_banked++;
    chain++;
    since_pickup = 0.f;
    score += 5;
    return chain;
}

void scoring_death(void) {
    shards_banked = 0;
    chain = 0;
    since_pickup = 0.f;
}

void scoring_update(float dt) {
    kill_pulse -= dt * 0.5f;
    if (kill_pulse < 0.f) kill_pulse = 0.f;

    /* Multiplier decays if the player stops collecting (GDD 3.4/8.1). */
    since_pickup += dt;
    if (since_pickup > DECAY_GRACE && shards_banked > 0) {
        decay_timer += dt;
        while (decay_timer >= DECAY_INTERVAL && shards_banked > 0) {
            decay_timer -= DECAY_INTERVAL;
            shards_banked--;
            chain = 0;   /* pitch ladder resets when the streak breaks */
        }
    } else {
        decay_timer = 0.f;
    }
}

uint32_t scoring_score(void)      { return score; }
float    scoring_kill_pulse(void) { return kill_pulse; }
