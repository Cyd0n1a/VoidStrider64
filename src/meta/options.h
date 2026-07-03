#pragma once
#include <stdbool.h>

/* Accessibility / comfort options (GDD 8.4) — first-class, not bolted
 * on. Persisted to EEPROM alongside the high scores (save.c). */
typedef struct {
    float bg_intensity;   /* 0.2..1: tunnel saturation/turbulence/roll */
    bool  reduce_flash;   /* caps rapid brightness changes (beats/bombs) */
} options_t;

extern options_t g_options;

void  options_init(void);
/* Multiplier applied to beat/bomb flash effects: 1, or 0.3 when
 * Reduce Flash is on. */
float options_flash_scale(void);
