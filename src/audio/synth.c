#include "synth.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 32000
#define MAX_VOICES  8

enum { W_SINE, W_SQUARE, W_SAW, W_NOISE };

/* Voice slots by role, so combat spam steals predictably (GDD 7.1
 * priority-based stealing, simplified: shots only ever steal shots). */
#define SLOT_SHOT0   0   /* shots round-robin 0..1 */
#define SLOT_DIE0    2   /* enemy deaths round-robin 2..4 */
#define SLOT_SHARD   5
#define SLOT_PDIE0   6   /* player death uses 6+7 (two layers) */

typedef struct {
    int   active;
    int   pos, len;
    float phase;
    float f0, f1;        /* linear pitch slide over the voice's life */
    int   wave;
    float volume;
    float lp_y, lp_a;    /* one-pole low-pass state + coefficient */
    float lp_decay;      /* per-sample multiplier on lp_a (filter sweep) */
} voice_t;

static voice_t  voices[MAX_VOICES];
static int      rr_shot, rr_die;
static uint32_t noise_lfsr = 0xACE1u;
static uint32_t rng_state  = 0x5EEDu;

static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static float frange(float a, float b) {
    return a + ((float)(rng() >> 8) / 16777216.f) * (b - a);
}

static void voice_start(int slot, const voice_t *cfg) {
    voices[slot]        = *cfg;
    voices[slot].active = 1;
    voices[slot].pos    = 0;
    voices[slot].phase  = 0.f;
    voices[slot].lp_y   = 0.f;
}

void synth_shot(void) {
    /* GDD 7.2: short square blip, fast downslide, per-shot pitch nudge
     * so rapid fire doesn't sound mechanically identical. */
    float nudge = frange(0.94f, 1.06f);
    voice_t v = {
        .len = (int)(SAMPLE_RATE * 0.055f),
        .f0 = 880.f * nudge, .f1 = 330.f * nudge,
        .wave = W_SQUARE, .volume = 0.16f, .lp_a = 1.f, .lp_decay = 1.f,
    };
    voice_start(SLOT_SHOT0 + rr_shot, &v);
    rr_shot ^= 1;
}

void synth_enemy_die(int gen) {
    /* Noise through a decaying low-pass; smaller enemies = shorter,
     * brighter burst (GDD 7.2: size scales the formula, not the asset). */
    float size = (gen == 0) ? 1.f : 0.55f;
    voice_t v = {
        .len = (int)(SAMPLE_RATE * (0.14f + 0.16f * size)),
        .f0 = 1.f, .f1 = 1.f,
        .wave = W_NOISE, .volume = 0.30f + 0.10f * size,
        .lp_a = 0.55f - 0.25f * size,       /* big = darker thump */
        .lp_decay = 0.99965f,
    };
    voice_start(SLOT_DIE0 + rr_die, &v);
    rr_die = (rr_die + 1) % 3;
}

void synth_shard(int chain) {
    /* Rising sine blip; base pitch steps up with the pickup chain and
     * resets when the multiplier drops (GDD 7.2). */
    if (chain > 24) chain = 24;
    float base = 520.f * powf(1.05946f, (float)chain);  /* semitone steps */
    voice_t v = {
        .len = (int)(SAMPLE_RATE * 0.09f),
        .f0 = base, .f1 = base * 1.6f,
        .wave = W_SINE, .volume = 0.20f, .lp_a = 1.f, .lp_decay = 1.f,
    };
    voice_start(SLOT_SHARD, &v);
}

void synth_player_die(void) {
    voice_t n = {
        .len = (int)(SAMPLE_RATE * 0.9f),
        .f0 = 1.f, .f1 = 1.f,
        .wave = W_NOISE, .volume = 0.42f,
        .lp_a = 0.6f, .lp_decay = 0.99988f,
    };
    voice_t s = {
        .len = (int)(SAMPLE_RATE * 0.8f),
        .f0 = 620.f, .f1 = 60.f,
        .wave = W_SINE, .volume = 0.30f, .lp_a = 1.f, .lp_decay = 1.f,
    };
    voice_start(SLOT_PDIE0,     &n);
    voice_start(SLOT_PDIE0 + 1, &s);
}

static void synth_mix_into(short *buf, int n_frames) {
    for (int i = 0; i < n_frames; i++) {
        float sample = 0.f;

        for (int vi = 0; vi < MAX_VOICES; vi++) {
            voice_t *vp = &voices[vi];
            if (!vp->active) continue;

            float t    = (float)vp->pos / (float)vp->len;
            float freq = vp->f0 + (vp->f1 - vp->f0) * t;

            vp->phase += freq / (float)SAMPLE_RATE;
            if (vp->phase >= 1.f) vp->phase -= 1.f;

            float osc;
            switch (vp->wave) {
                case W_SINE:   osc = sinf(vp->phase * 6.28318f);        break;
                case W_SQUARE: osc = (vp->phase < 0.5f) ? 1.f : -1.f;  break;
                case W_SAW:    osc = vp->phase * 2.f - 1.f;            break;
                default: {  /* 16-bit Galois LFSR noise (GDD 7.1) */
                    noise_lfsr = (noise_lfsr >> 1) ^ (-(noise_lfsr & 1u) & 0xB400u);
                    osc = (float)(int)(noise_lfsr & 0xFFFF) / 32768.f - 1.f;
                    break;
                }
            }

            /* One-pole low-pass with decaying coefficient: turns noise
             * into "thump" without FFT-priced effects (GDD 7.1). */
            vp->lp_a *= vp->lp_decay;
            vp->lp_y += vp->lp_a * (osc - vp->lp_y);

            float env = 1.f - t * t;
            sample += vp->lp_y * env * vp->volume;

            vp->pos++;
            if (vp->pos >= vp->len) vp->active = 0;
        }

        if (sample >  1.f) sample =  1.f;
        if (sample < -1.f) sample = -1.f;

        short s = (short)(sample * 26000.f);
        int L = (int)buf[i * 2]     + s;
        int R = (int)buf[i * 2 + 1] + s;
        buf[i * 2]     = (short)(L > 32767 ? 32767 : L < -32768 ? -32768 : L);
        buf[i * 2 + 1] = (short)(R > 32767 ? 32767 : R < -32768 ? -32768 : R);
    }
}

void synth_init(void) {
    memset(voices, 0, sizeof(voices));
    rr_shot = rr_die = 0;
    audio_init(SAMPLE_RATE, 4);
    mixer_init(16);   /* headroom for xm64 music channels in M4 (GDD 7.3) */
}

void synth_poll(void) {
    while (audio_can_write()) {
        short *buf = audio_write_begin();
        int n = audio_get_buffer_length();
        mixer_poll(buf, n);
        synth_mix_into(buf, n);
        audio_write_end();
    }
}
