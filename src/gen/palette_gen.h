#pragma once
#include <stdint.h>

/* HSV -> packed RGBA8888 (0xRRGGBBAA, alpha 0xFF).
 * h wraps to [0,1); s, v clamped to [0,1].  GDD 5.3: all game color comes
 * from HSV wheel walks, so this is the single color source of truth. */
uint32_t palette_hsv_rgba(float h, float s, float v);

/* Slowly-drifting base hue for the whole scene (GDD 5.3: palette drifts
 * across the color wheel over the course of a run). */
float palette_base_hue(float time);
