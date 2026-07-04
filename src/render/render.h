#pragma once
#include <libdragon.h>
#include "../sim/player.h"
#include "render_ui.h"

void render_init(void);
void render_frame(surface_t *disp, float time, const player_t *player,
                  const hud_state_t *hud);

/* Frame-budget director readouts for the ABOUT screen (GDD 9.2): the
 * rolling average frame time it steers by, and whether it has currently
 * shed background detail to stay on budget. */
float render_frame_ms(void);
bool  render_fb_reduced(void);
