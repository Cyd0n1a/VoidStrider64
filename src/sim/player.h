#pragma once
#include "../input/input.h"

typedef struct {
    float x, y;
    float vx, vy;
    float heading;      /* radians; direction the ship points */
    float speed_norm;   /* |vel| / max speed, for thruster/grid effects */
} player_t;

void player_init(player_t *p);
void player_update(player_t *p, const input_state_t *inp, float dt);
