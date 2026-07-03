#pragma once
#include <stdint.h>

void render_entities_init(void);

/* Regenerate the species mesh atlas from a cosmetic seed — the "remix"
 * look of the whole bestiary changes per run (GDD 5.2/8.3). Drains the
 * RSP first; call only at run boundaries. */
void render_entities_reseed(uint32_t seed);

/* Rewrite this frame's dynamic buffers (bullet/shard verts, enemy
 * matrices). Call before the t3d frame starts. */
void render_entities_build(int fi, float time);

/* Draw enemies, bullets and shards. Call inside the gameplay layer
 * (opaque, after the grid). Leaves combiner set to SHADE. */
void render_entities_draw(int fi, float time);
