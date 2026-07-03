#pragma once

/* The combat playfield: a flat X/Y plane floating inside the tunnel at a
 * fixed Z (GDD 1.1 #2 — depth is illusion; gameplay is strictly 2D).
 * Sized to sit visually inside the tube opening so the tunnel wraps
 * around and shows through the grid. */
#define ARENA_HALF_W  150.f
#define ARENA_HALF_H  110.f
#define ARENA_Z       (-300.f)
