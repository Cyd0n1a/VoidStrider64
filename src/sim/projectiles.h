#pragma once
#include <stdbool.h>
#include "../input/input.h"
#include "player.h"

#define MAX_BULLETS 40

typedef struct {
    bool  alive;
    float x, y, vx, vy;
} bullet_t;

extern bullet_t bullets[MAX_BULLETS];

void projectiles_init(void);
void projectiles_update(float dt);
void projectiles_clear(void);

/* C-button 8-way aim/fire (GDD 4): fires continuously while any C-button
 * is held; an opposing-pair press keeps the last held direction alive
 * ("soft lock") so strafing doesn't drop the stream. */
void projectiles_fire_tick(const input_state_t *inp, const player_t *p, float dt);
