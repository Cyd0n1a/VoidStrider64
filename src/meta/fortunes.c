#include "fortunes.h"

static const char *const FORTUNES[] = {
#include "fortunes_data.h"
};
#define FORTUNES_COUNT (sizeof(FORTUNES) / sizeof(FORTUNES[0]))

const char *fortune_random(uint32_t entropy) {
    /* Scramble so consecutive timer values don't pick neighbors. */
    entropy ^= entropy << 13;
    entropy ^= entropy >> 17;
    entropy ^= entropy << 5;
    return FORTUNES[entropy % FORTUNES_COUNT];
}
