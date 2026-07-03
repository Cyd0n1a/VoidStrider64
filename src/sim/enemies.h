#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MAX_ENEMIES 24

/* Wanderer (GDD 3.1): rotating tetrahedron, noise-driven drift, splits
 * into two smaller ones on death. More species arrive in M3. */
typedef struct {
    bool  alive;
    int   gen;              /* 0 = full size, 1 = split child */
    float x, y;
    float heading;          /* drift direction, wobbled by an LFO */
    float wander_phase, wander_spd;
    float spin;             /* current rotation angle */
    float spin_spd;
    float axis[3];          /* per-spawn rotation axis (normalized) */
    float jit[3];           /* per-spawn scale jitter (GDD 3.1) */
    float spawn_t;          /* fade-in grace: no collisions while > 0 */
} enemy_t;

extern enemy_t enemies[MAX_ENEMIES];

void enemies_init(uint32_t seed);
void enemies_update(float dt);
int  enemies_spawn(float x, float y, int gen);   /* -1 if pool full */
void enemies_kill(int idx);                      /* split + fx + scoring */
void enemies_clear(void);                        /* on player death */
int  enemies_count(void);
float enemy_radius(const enemy_t *e);
