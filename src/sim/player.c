#include "player.h"
#include "arena.h"
#include <libdragon.h>
#include <math.h>

#define P_MAXSP    260.f   /* units/sec — crosses the arena in ~1.2s */
#define P_RESPONSE 6.f     /* velocity approach rate (higher = snappier) */
#define P_TURN     14.f    /* heading approach rate, rad/sec-ish */
#define P_RADIUS   10.f    /* keeps the hull inside the arena walls */

void player_init(player_t *p) {
    p->x = 0.f;  p->y = 0.f;
    p->vx = 0.f; p->vy = 0.f;
    p->heading = 1.5707963f;  /* facing "up" */
    p->speed_norm = 0.f;
}

void player_update(player_t *p, const input_state_t *inp, float dt) {
    /* Velocity chases stick*max: analog-proportional speed with a little
     * inertia, GW-style responsive but not teleporty (GDD 4). */
    float blend = dt * P_RESPONSE;
    if (blend > 1.f) blend = 1.f;
    p->vx += (inp->move_x * P_MAXSP - p->vx) * blend;
    p->vy += (inp->move_y * P_MAXSP - p->vy) * blend;

    p->x += p->vx * dt;
    p->y += p->vy * dt;

    if (p->x < -ARENA_HALF_W + P_RADIUS) { p->x = -ARENA_HALF_W + P_RADIUS; p->vx = 0.f; }
    if (p->x >  ARENA_HALF_W - P_RADIUS) { p->x =  ARENA_HALF_W - P_RADIUS; p->vx = 0.f; }
    if (p->y < -ARENA_HALF_H + P_RADIUS) { p->y = -ARENA_HALF_H + P_RADIUS; p->vy = 0.f; }
    if (p->y >  ARENA_HALF_H - P_RADIUS) { p->y =  ARENA_HALF_H - P_RADIUS; p->vy = 0.f; }

    float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
    p->speed_norm = speed / P_MAXSP;
    if (p->speed_norm > 1.f) p->speed_norm = 1.f;

    /* Face travel direction once actually moving (aim comes in M2 and is
     * independent of facing per the C-button scheme, GDD 4). */
    if (speed > 30.f) {
        float target = atan2f(p->vy, p->vx);
        float diff = target - p->heading;
        while (diff >  3.1415927f) diff -= 6.2831853f;
        while (diff < -3.1415927f) diff += 6.2831853f;
        float tb = dt * P_TURN;
        if (tb > 1.f) tb = 1.f;
        p->heading += diff * tb;
    }
}
