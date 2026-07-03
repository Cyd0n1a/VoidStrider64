#include "options.h"

options_t g_options;

void options_init(void) {
    g_options.bg_intensity = 1.f;
    g_options.reduce_flash = false;
}

float options_flash_scale(void) {
    return g_options.reduce_flash ? 0.3f : 1.f;
}
