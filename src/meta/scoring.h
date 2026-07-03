#pragma once
#include <stdint.h>

void     scoring_init(void);
void     scoring_update(float dt);

void     scoring_kill(int gen);   /* enemy killed; points scale by class/gen x mult */
int      scoring_shard(void);     /* shard collected; returns pickup chain for SFX pitch */
void     scoring_death(void);     /* player died: multiplier + chain reset (GDD 8.1) */

uint32_t scoring_score(void);
int      scoring_mult(void);      /* 1..10 */

/* Decaying pulse fed by recent kills; drives tunnel intensity until the
 * full director metric lands in M3 (GDD 6.2). */
float    scoring_kill_pulse(void);
