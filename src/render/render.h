#pragma once
#include <libdragon.h>
#include "../sim/player.h"

void render_init(void);
void render_frame(surface_t *disp, float time, const player_t *player);
