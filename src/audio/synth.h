#pragma once

/* Procedural SFX synth (GDD 7.1/7.2): every sound is synthesized into the
 * audio buffer in real time — no samples. Parameters are driven by small
 * formulas keyed off gameplay state. Music (.xm64) arrives in M4 and will
 * share the same mixer. */
void synth_init(void);
void synth_poll(void);

void synth_shot(void);           /* square blip, pitch downslide, random nudge */
void synth_enemy_die(int gen);   /* noise burst through decaying LP; gen scales size */
void synth_shard(int chain);     /* rising sine blip, pitch steps with combo chain */
void synth_player_die(void);     /* layered noise + descending sine sweep */
