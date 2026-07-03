#pragma once
#include <stdint.h>

/* Random MOTD fortunes for the pause/game-over marquee, generated at
 * build time from assets/fortunes.md by tools/gen_fortunes.py. */
const char *fortune_random(uint32_t entropy);
