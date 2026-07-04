#pragma once
#include <libdragon.h>
#include <stdbool.h>

/* Boot splash sequence: libdragon logo (drop/spin/fade in), crossfade to
 * the Cydonis logo (per-pixel column sine wave), then fade into the live
 * title scene. Authored sprites/sounds are a sanctioned exception to the
 * "nothing stored" pillar, contained to this module (like the music). */
void splash_init(void);

/* Advance the timeline; call once per sim step (no-op once finished). */
void splash_update(float dt);

/* Abort the whole sequence (any-button skip). */
void splash_skip(void);

/* True while the splash owns the whole frame (render via splash_render).
 * The final fade renders as an overlay on the title scene instead. */
bool splash_fullscreen(void);
bool splash_finished(void);

/* Full-frame splash render: attaches, draws, presents. */
void splash_render(surface_t *disp);

/* Fade-to-title overlay; render.c calls this after the UI pass on the
 * title screen. No-op outside the fade-out window. */
void splash_draw_overlay(void);
