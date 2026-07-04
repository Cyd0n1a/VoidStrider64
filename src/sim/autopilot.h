#pragma once
#include "../input/input.h"
#include "player.h"

/* Attract-mode pilot (demo mode): synthesizes a controller state from
 * the live sim each step, so the demo runs through the exact same
 * player/fire/bomb code paths as a human run. */
void autopilot_reset(void);
void autopilot_drive(input_state_t *out, const player_t *p, float dt);
