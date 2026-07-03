#pragma once
#include <stdbool.h>

/* Smart bomb (GDD 8.2): charge fills slowly over time, faster via shard
 * pickups; manual detonation on Z clears the screen and grants brief
 * invulnerability (GDD 3.5). Starts a run fully charged. */
void  bomb_init(void);
void  bomb_update(float dt);
void  bomb_notify_shard(void);
bool  bomb_try_fire(void);    /* true if charge was full; resets it */
float bomb_charge(void);      /* 0..1 for the HUD meter */
