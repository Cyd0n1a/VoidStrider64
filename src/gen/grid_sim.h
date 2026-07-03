#pragma once
#include <stdint.h>
#include <t3d/t3d.h>

/* The transparent arena grid (GDD 6.3): a plane of quads with bright
 * intersection verts and dark cell-center verts, so grid lines glow and
 * cell interiors stay see-through over the tunnel.
 *
 * Verts are emitted in per-row draw batches so each batch is one
 * contiguous t3d_vert_load: row r intersections (13), row r+1
 * intersections (13), row r cell centers (12) = 38 verts = 19 packed.
 * Intersection rows are duplicated across batches — a few hundred bytes
 * traded for single-DMA batches. */
#define GRID_COLS               16
#define GRID_ROWS               12
#define GRID_IX_W               (GRID_COLS + 1)
#define GRID_IX_H               (GRID_ROWS + 1)
#define GRID_VERTS_PER_BATCH    (2 * GRID_IX_W + GRID_COLS)      /* 38 */
#define GRID_PACKED_PER_BATCH   (GRID_VERTS_PER_BATCH / 2)       /* 19 */
#define GRID_PACKED_COUNT       (GRID_ROWS * GRID_PACKED_PER_BATCH)

void grid_init(void);
void grid_update(float dt);

/* Radial ripple: pushes nearby intersections away from the camera with
 * strength falling off linearly to `radius` (GDD 6.3 explosions). */
void grid_impulse(float x, float y, float strength, float radius);

void grid_build_verts(T3DVertPacked *dst, float time);
