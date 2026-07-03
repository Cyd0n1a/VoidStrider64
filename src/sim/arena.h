#pragma once

/* The combat playfield: a flat X/Y plane floating inside the tunnel at a
 * fixed Z (GDD 1.1 #2 — depth is illusion; gameplay is strictly 2D).
 * Extends past the tube opening at the edges; the layered compositing
 * keeps the tunnel behind it regardless. */
#define ARENA_HALF_W  225.f
#define ARENA_HALF_H  165.f
#define ARENA_Z       (-300.f)
