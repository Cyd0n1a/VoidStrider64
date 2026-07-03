#pragma once
#include <stdbool.h>
#include <stdint.h>

/* EEPROM persistence (GDD 8.5): top scores with their seeds — so a
 * great run's look/challenge can be shared and replayed (GDD 8.3) —
 * plus the accessibility options. Fits comfortably in 4Kbit EEPROM. */
#define SAVE_TOP_N 5

typedef struct {
    uint32_t score;
    uint32_t cseed, dseed;
    uint32_t secs;          /* survival time */
} hs_entry_t;

void save_init(void);       /* call after options_init; loads + applies */
bool save_available(void);  /* false when no EEPROM (emulator misconfig) */

const hs_entry_t *save_top(int rank);   /* rank 0..SAVE_TOP_N-1 */
uint32_t save_hi_score(void);

/* Insert a finished run; returns its rank (0-based) or -1 if it didn't
 * place. Writes EEPROM when it places. */
int save_submit(uint32_t score, uint32_t cseed, uint32_t dseed, uint32_t secs);

/* Persist the current g_options. */
void save_options_sync(void);
