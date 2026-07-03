#include "synth.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>

#define SAMPLE_RATE 32000
#define MAX_VOICES  10

enum { W_SINE, W_SQUARE, W_SAW, W_NOISE };

/* Voice priorities (GDD 7.1: priority-based stealing when the pool is
 * full during heavy combat). A new sound may steal an inactive slot,
 * else the most-progressed voice of equal-or-lower priority; if
 * everything live outranks it, the new sound is dropped. */
#define PRIO_SHOT   1
#define PRIO_EBOLT  1
#define PRIO_SHARD  2
#define PRIO_DIE    3
#define PRIO_BIG    5   /* player death / bomb layers: never stolen */

typedef struct {
    int   active;
    int   pos, len;
    float phase;
    float f0, f1;        /* linear pitch slide over the voice's life */
    int   wave;
    float volume;
    float lp_y, lp_a;    /* one-pole low-pass state + coefficient */
    float lp_decay;      /* per-sample multiplier on lp_a (filter sweep) */
    int   prio;
} voice_t;

static voice_t  voices[MAX_VOICES];
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

static void voice_start(int prio, const voice_t *cfg) {
    int steal = -1;
    float steal_score = -1.f;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) { steal = i; break; }
        if (voices[i].prio > prio) continue;
        /* Prefer stealing lower priority, then the most-finished voice. */
        float score = (float)(prio - voices[i].prio) * 10.f
                    + (float)voices[i].pos / (float)voices[i].len;
        if (score > steal_score) { steal_score = score; steal = i; }
    }
    if (steal < 0) return;   /* pool full of more important sounds */

    voices[steal]        = *cfg;
    voices[steal].active = 1;
    voices[steal].pos    = 0;
    voices[steal].phase  = 0.f;
    voices[steal].lp_y   = 0.f;
    voices[steal].prio   = prio;
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
    voice_start(PRIO_SHOT, &v);
}

void synth_enemy_die(float size) {
    /* Noise through a decaying low-pass; smaller enemies = shorter,
     * brighter burst (GDD 7.2: size scales the formula, not the asset). */
    voice_t v = {
        .len = (int)(SAMPLE_RATE * (0.14f + 0.16f * size)),
        .f0 = 1.f, .f1 = 1.f,
        .wave = W_NOISE, .volume = 0.30f + 0.10f * size,
        .lp_a = 0.55f - 0.25f * size,       /* big = darker thump */
        .lp_decay = 0.99965f,
    };
    voice_start(PRIO_DIE, &v);
}

void synth_ebolt(void) {
    float nudge = frange(0.92f, 1.08f);
    voice_t v = {
        .len = (int)(SAMPLE_RATE * 0.08f),
        .f0 = 300.f * nudge, .f1 = 140.f * nudge,
        .wave = W_SAW, .volume = 0.13f, .lp_a = 1.f, .lp_decay = 1.f,
    };
    voice_start(PRIO_EBOLT, &v);
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
    voice_start(PRIO_SHARD, &v);
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
    voice_start(PRIO_BIG, &n);
    voice_start(PRIO_BIG, &s);
}

void synth_bomb(void) {
    /* GDD 7.2: layered noise + descending sine sweep, duration matched
     * to the screen-clear moment. Shares the player-death slot pair. */
    voice_t n = {
        .len = (int)(SAMPLE_RATE * 1.1f),
        .f0 = 1.f, .f1 = 1.f,
        .wave = W_NOISE, .volume = 0.45f,
        .lp_a = 0.8f, .lp_decay = 0.99993f,
    };
    voice_t s = {
        .len = (int)(SAMPLE_RATE * 1.0f),
        .f0 = 340.f, .f1 = 32.f,
        .wave = W_SINE, .volume = 0.34f, .lp_a = 1.f, .lp_decay = 1.f,
    };
    voice_start(PRIO_BIG, &n);
    voice_start(PRIO_BIG, &s);
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

/* ---- Live music analysis (GDD 6.2/6.3) ----
 * Runs on the mixer output BEFORE the SFX synth adds its samples, so it
 * hears music only. Energy-flux onset detection: a beat fires when the
 * chunk RMS jumps well above its slow-moving average, with a refractory
 * gap so one kick = one beat. Tracks tempo changes across the 27-minute
 * mix automatically (fixed-BPM clocks drift: the mix spans ~70-140bpm). */
static float beat_pulse_v;
static float music_level_v;
static float energy_avg;
static int   since_beat = SAMPLE_RATE;

float synth_beat_pulse(void)  { return beat_pulse_v; }
float synth_music_level(void) { return music_level_v; }

static void analyze_music(const short *buf, int n_frames) {
    float sum = 0.f;
    int   taken = 0;
    for (int i = 0; i < n_frames; i += 4) {   /* sparse sampling is plenty */
        float l = (float)buf[i * 2]     * (1.f / 32768.f);
        float r = (float)buf[i * 2 + 1] * (1.f / 32768.f);
        sum += l * l + r * r;
        taken++;
    }
    float rms = sqrtf(sum / (float)(taken * 2));
    float dt  = (float)n_frames / (float)SAMPLE_RATE;

    /* Onset test against the slow average, then update the average. */
    if (rms > energy_avg * 1.35f + 0.012f && since_beat > SAMPLE_RATE / 4) {
        beat_pulse_v = 1.f;
        since_beat   = 0;
    }
    since_beat += n_frames;

    float blend = dt * 2.5f;
    if (blend > 1.f) blend = 1.f;
    energy_avg += (rms - energy_avg) * blend;

    float lvl = rms * 2.4f;
    if (lvl > 1.f) lvl = 1.f;
    float lblend = dt * 6.f;
    if (lblend > 1.f) lblend = 1.f;
    music_level_v += (lvl - music_level_v) * lblend;

    beat_pulse_v *= expf(-7.f * dt);
}

void synth_init(void) {
    memset(voices, 0, sizeof(voices));
    audio_init(SAMPLE_RATE, 4);
    mixer_init(32);   /* wav64 music on 0-1, 24-channel title XM on 2-25 */
}

void synth_poll(void) {
    while (audio_can_write()) {
        short *buf = audio_write_begin();
        int n = audio_get_buffer_length();
        mixer_poll(buf, n);
        analyze_music(buf, n);   /* music only: SFX not yet mixed in */
        synth_mix_into(buf, n);
        audio_write_end();
    }
}
