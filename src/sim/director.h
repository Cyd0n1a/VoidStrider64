#pragma once
#include <stdint.h>

/* Spawn pacing + difficulty curve (GDD 9.4). M2 version: a single ramp
 * for the Wanderer; per-species waves and the full intensity metric
 * arrive in M3. */
void  director_init(uint32_t seed);
void  director_update(float dt, float px, float py);
float director_intensity(void);   /* 0..1, drives the tunnel (GDD 6.2) */
