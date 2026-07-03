#pragma once

/* Music routing (revised from GDD 7.3 by design decision 2026-07-03):
 *  - Title/options screens: hand-composed .xm tracker module streamed
 *    via libdragon's xm64player.
 *  - Gameplay + pause: the full soundtrack mix as a VADPCM .wav64
 *    streamed from ROM, looped.
 * Both share the libdragon mixer with the procedural SFX synth (which
 * writes raw samples and never touches mixer channels). */
void music_init(void);       /* after synth_init (audio/mixer must exist) */
void music_title(void);
void music_gameplay(void);
void music_stop(void);
