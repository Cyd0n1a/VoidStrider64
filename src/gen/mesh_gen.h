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

/* Enemy species meshes (GDD 3.1): white verts with per-vertex brightness
 * variation faking facet shading — class color comes from prim color at
 * draw time so it can ride the HSV wheel walk (GDD 5.3). All generators
 * take a seed for per-variant jitter. */

/* Wanderer / Snake segment: tetrahedron. 4 verts = 2 packed, 4 tris. */
#define TETRA_VERTS         4
#define TETRA_PACKED_COUNT  2
void mesh_gen_tetra(T3DVertPacked *out, float radius, uint32_t seed);
void mesh_tetra_draw(void);

/* Seeker: elongated diamond (stretched octahedron). 6 verts = 3 packed. */
#define SEEKER_VERTS        6
#define SEEKER_PACKED_COUNT 3
void mesh_gen_seeker(T3DVertPacked *out, uint32_t seed);
void mesh_seeker_draw(void);

/* Swarmer: small flat triangle (double-sided via no-cull). 4 verts = 2 packed. */
#define SWARMER_VERTS        4
#define SWARMER_PACKED_COUNT 2
void mesh_gen_swarmer(T3DVertPacked *out, uint32_t seed);
void mesh_swarmer_draw(void);

/* Turret: hexagon plate with raised center. 8 verts = 4 packed, 6 tris. */
#define TURRET_VERTS        8
#define TURRET_PACKED_COUNT 4
void mesh_gen_turret(T3DVertPacked *out, uint32_t seed);
void mesh_turret_draw(void);

/* Pulsar: spiky octahedron ("icosahedron-ish cluster" on a triangle
 * budget). 6 verts = 3 packed, 8 tris. */
#define PULSAR_VERTS        6
#define PULSAR_PACKED_COUNT 3
void mesh_gen_pulsar(T3DVertPacked *out, uint32_t seed);
void mesh_pulsar_draw(void);
