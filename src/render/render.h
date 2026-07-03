#pragma once
#include <libdragon.h>
#include "../sim/player.h"
#include "render_ui.h"

void render_init(void);
void render_frame(surface_t *disp, float time, const player_t *player,
                  const hud_state_t *hud);
