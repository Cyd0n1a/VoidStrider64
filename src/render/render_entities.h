#pragma once
#include <stdint.h>

void render_entities_init(void);

/* Rewrite this frame's dynamic buffers (bullet/shard verts, enemy
 * matrices). Call before the t3d frame starts. */
void render_entities_build(int fi, float time);

/* Draw enemies, bullets and shards. Call inside the gameplay layer
 * (opaque, after the grid). Leaves combiner set to SHADE. */
void render_entities_draw(int fi, float time);
