#pragma once
#include <stdint.h>
#include <t3d/t3d.h>

/* GDD 5.4: ring-based tunnel, ~16-24 sides x rolling window of ~24-32 rings. */
#define TUNNEL_SIDES            16
#define TUNNEL_RINGS            24
#define TUNNEL_SEGS             (TUNNEL_RINGS - 1)
#define TUNNEL_DZ               64.f     /* spacing between rings along -Z */
#define TUNNEL_NEAR_Z           (-72.f)  /* nearest ring's Z at scroll = 0 */
#define TUNNEL_PACKED_PER_RING  (TUNNEL_SIDES / 2)
#define TUNNEL_PACKED_COUNT     (TUNNEL_RINGS * TUNNEL_PACKED_PER_RING)

void  tunnel_init(uint32_t seed);

/* intensity in [0,1] drives flight speed, wall turbulence and roll rate
 * (GDD 6.2: tunnel reacts to game intensity). */
void  tunnel_update(float dt, float intensity);

/* Rewrite the vertex window for this frame (verts are cheap: ~384).
 * dst must hold TUNNEL_PACKED_COUNT packed verts, slot-ordered near->far.
 * n_rings <= TUNNEL_RINGS lets the frame-budget director shrink the
 * window under load (GDD 9.2); rings beyond it are left unwritten. */
void  tunnel_build_verts(T3DVertPacked *dst, float time, int n_rings);

/* Current view-axis roll angle (radians) for the tunnel model matrix
 * (GDD 6.2: chase-cam roll applies to the tunnel only, never the grid). */
float tunnel_roll(void);
