#pragma once

/* Procedural SFX synth (GDD 7.1/7.2): every sound is synthesized into the
 * audio buffer in real time — no samples. Parameters are driven by small
 * formulas keyed off gameplay state. Music (.xm64) arrives in M4 and will
 * share the same mixer. */
void synth_init(void);
void synth_poll(void);

/* Live music analysis (GDD 6.2/6.3): the mixer's output is measured
 * before SFX are added, so these react to music only — works for both
 * the title XM and the gameplay wav64, and follows tempo changes. */
float synth_beat_pulse(void);     /* 1 at a detected beat, exp decay to 0 */
float synth_music_level(void);    /* smoothed music energy, roughly 0..1 */

void synth_shot(void);             /* square blip, pitch downslide, random nudge */
void synth_enemy_die(float size);  /* noise burst through decaying LP; size 0..~1.3
                                      scales duration/darkness (GDD 7.2) */
void synth_ebolt(void);            /* enemy bolt: low saw blip */
void synth_shard(int chain);     /* rising sine blip, pitch steps with combo chain */
void synth_player_die(void);     /* layered noise + descending sine sweep */
void synth_bomb(void);           /* longer, deeper layered sweep (GDD 7.2) */
