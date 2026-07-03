#include "palette_gen.h"
#include <math.h>

uint32_t palette_hsv_rgba(float h, float s, float v) {
    h -= floorf(h);
    if (s < 0.f) s = 0.f; else if (s > 1.f) s = 1.f;
    if (v < 0.f) v = 0.f; else if (v > 1.f) v = 1.f;

    float hf = h * 6.f;
    int   hi = (int)hf;
    float f  = hf - (float)hi;
    float p  = v * (1.f - s);
    float q  = v * (1.f - s * f);
    float t  = v * (1.f - s * (1.f - f));

    float r, g, b;
    switch (hi % 6) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return ((uint32_t)(r * 255.f) << 24) |
           ((uint32_t)(g * 255.f) << 16) |
           ((uint32_t)(b * 255.f) <<  8) | 0xFF;
}

float palette_base_hue(float time) {
    /* Full trip around the wheel roughly every 45 seconds. */
    return time * (1.f / 45.f);
}
