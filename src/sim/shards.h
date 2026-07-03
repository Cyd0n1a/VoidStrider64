#pragma once
#include <stdbool.h>

#define MAX_SHARDS 48

/* Geometric shards (GDD 3.4): dropped on kills, magnet toward the player
 * when close, build the multiplier when collected, expire otherwise. */
typedef struct {
    bool  alive;
    float x, y, vx, vy;
    float life;          /* seconds remaining */
    float spin;
} shard_t;

extern shard_t shards[MAX_SHARDS];

void shards_init(void);
void shards_update(float dt, float px, float py);
void shards_burst(float x, float y, int count);
void shards_clear(void);
