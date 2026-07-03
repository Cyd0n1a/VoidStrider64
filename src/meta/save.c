#include "save.h"
#include "options.h"
#include <libdragon.h>
#include <string.h>

#define SAVE_MAGIC 0x56535231u   /* "VSR1" */
#define SAVE_PATH  "/save.dat"

typedef struct {
    uint32_t   magic;
    uint8_t    bg_pct;          /* bg_intensity * 100 */
    uint8_t    reduce_flash;
    uint8_t    pad[2];
    hs_entry_t top[SAVE_TOP_N];
    uint32_t   checksum;
} save_blob_t;

static save_blob_t blob;
static bool        save_ok;

static uint32_t blob_checksum(const save_blob_t *b) {
    const uint32_t *w = (const uint32_t *)b;
    size_t n = (sizeof(*b) - sizeof(uint32_t)) / sizeof(uint32_t);
    uint32_t sum = 0x600DF00Du;
    for (size_t i = 0; i < n; i++)
        sum = (sum << 1 | sum >> 31) ^ w[i];
    return sum;
}

static void blob_defaults(void) {
    memset(&blob, 0, sizeof(blob));
    blob.magic  = SAVE_MAGIC;
    blob.bg_pct = 100;
}

static void blob_write(void) {
    if (!save_ok) return;
    blob.checksum = blob_checksum(&blob);
    eepfs_write(SAVE_PATH, &blob, sizeof(blob));
}

void save_init(void) {
    static const eepfs_entry_t entries[] = {
        { SAVE_PATH, sizeof(save_blob_t) },
    };
    save_ok = (eepfs_init(entries, 1) == 0);   /* fails without EEPROM */
    if (!save_ok) {
        blob_defaults();
        return;
    }

    if (!eepfs_verify_signature()) {
        /* First boot (or another game's data): start fresh. */
        eepfs_wipe();
        blob_defaults();
        blob_write();
    } else {
        eepfs_read(SAVE_PATH, &blob, sizeof(blob));
        if (blob.magic != SAVE_MAGIC || blob.checksum != blob_checksum(&blob)) {
            blob_defaults();
            blob_write();
        }
    }

    /* Apply persisted options (GDD 8.4). */
    g_options.bg_intensity = (float)blob.bg_pct / 100.f;
    if (g_options.bg_intensity < 0.2f) g_options.bg_intensity = 0.2f;
    if (g_options.bg_intensity > 1.f)  g_options.bg_intensity = 1.f;
    g_options.reduce_flash = blob.reduce_flash != 0;
}

bool save_available(void) { return save_ok; }

const hs_entry_t *save_top(int rank) {
    if (rank < 0 || rank >= SAVE_TOP_N) return NULL;
    return &blob.top[rank];
}

uint32_t save_hi_score(void) { return blob.top[0].score; }

int save_submit(uint32_t score, uint32_t cseed, uint32_t dseed, uint32_t secs) {
    if (score == 0) return -1;
    int rank = -1;
    for (int i = 0; i < SAVE_TOP_N; i++) {
        if (score > blob.top[i].score) { rank = i; break; }
    }
    if (rank < 0) return -1;

    for (int i = SAVE_TOP_N - 1; i > rank; i--)
        blob.top[i] = blob.top[i - 1];
    blob.top[rank] = (hs_entry_t){ score, cseed, dseed, secs };
    blob_write();
    return rank;
}

void save_options_sync(void) {
    uint8_t pct = (uint8_t)(g_options.bg_intensity * 100.f + 0.5f);
    uint8_t rf  = g_options.reduce_flash ? 1 : 0;
    if (pct == blob.bg_pct && rf == blob.reduce_flash) return;
    blob.bg_pct       = pct;
    blob.reduce_flash = rf;
    blob_write();
}
