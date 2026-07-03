#include "projectiles.h"
#include "arena.h"
#include "../audio/synth.h"

#define BULLET_SPEED 500.f
#define FIRE_PERIOD  0.11f   /* ~9 shots/sec */
#define MUZZLE_OFF   14.f

bullet_t bullets[MAX_BULLETS];

static float fire_cooldown;
static float aim_x, aim_y;    /* soft-lock: last valid aim direction */

void projectiles_init(void) {
    projectiles_clear();
    fire_cooldown = 0.f;
    aim_x = 0.f; aim_y = 1.f;
}

void projectiles_clear(void) {
    for (int i = 0; i < MAX_BULLETS; i++)
        bullets[i].alive = false;
}

static void fire(const player_t *p) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].alive) continue;
        bullets[i].alive = true;
        bullets[i].x  = p->x + aim_x * MUZZLE_OFF;
        bullets[i].y  = p->y + aim_y * MUZZLE_OFF;
        bullets[i].vx = aim_x * BULLET_SPEED;
        bullets[i].vy = aim_y * BULLET_SPEED;
        synth_shot();
        return;
    }
}

void projectiles_fire_tick(const input_state_t *inp, const player_t *p, float dt) {
    if (fire_cooldown > 0.f) fire_cooldown -= dt;

    bool any = inp->c_up || inp->c_down || inp->c_left || inp->c_right;
    if (!any) return;

    int dx = (inp->c_right ? 1 : 0) - (inp->c_left ? 1 : 0);
    int dy = (inp->c_up    ? 1 : 0) - (inp->c_down ? 1 : 0);
    if (dx != 0 || dy != 0) {
        /* Normalize the 8-way combo; diagonals are 1/sqrt(2). */
        float inv = (dx != 0 && dy != 0) ? 0.7071068f : 1.f;
        aim_x = (float)dx * inv;
        aim_y = (float)dy * inv;
    }
    /* dx==dy==0 with buttons held: opposing pair — keep soft-locked aim. */

    if (fire_cooldown <= 0.f) {
        fire(p);
        fire_cooldown += FIRE_PERIOD;
    }
}

void projectiles_update(float dt) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive) continue;
        bullets[i].x += bullets[i].vx * dt;
        bullets[i].y += bullets[i].vy * dt;
        if (bullets[i].x < -ARENA_HALF_W || bullets[i].x > ARENA_HALF_W ||
            bullets[i].y < -ARENA_HALF_H || bullets[i].y > ARENA_HALF_H)
            bullets[i].alive = false;
    }
}
