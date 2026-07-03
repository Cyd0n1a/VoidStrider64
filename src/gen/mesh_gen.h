#pragma once
#include <stdint.h>
#include <t3d/t3d.h>

/* Player ship: arrowhead/star hybrid built in model space (forward = +Y,
 * camera side = +Z), GDD 5.2. Fixed baseline seed keeps the silhouette
 * recognizable run to run; jitter is cosmetic only.
 *
 * Layout in the output buffer (verts, 2 per packed entry):
 *   0..5  hull (static after generation)
 *   6..8  thruster triangle (rewritten every frame by the renderer)
 *   9     padding
 */
#define SHIP_HULL_VERTS     6
#define SHIP_TOTAL_VERTS    10
#define SHIP_PACKED_COUNT   (SHIP_TOTAL_VERTS / 2)
#define SHIP_THRUST_BASE_L  (-4.f)
#define SHIP_THRUST_BASE_R  (4.f)
#define SHIP_THRUST_BASE_Y  (-8.f)

void mesh_gen_ship(T3DVertPacked *out, uint32_t seed);

/* Emit the hull triangle indices into a recorded block (8 tris). */
void mesh_ship_draw_hull(void);
