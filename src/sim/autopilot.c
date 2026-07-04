#include "autopilot.h"
#include "arena.h"
#include "enemies.h"
#include "projectiles.h"
#include "shards.h"
#include "bomb.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>

/* tan(22.5 deg): 8-way aim sectors matching the C-button scheme (GDD 4). */
#define SECTOR_TAN 0.4142136f

static float wander_t;

void autopilot_reset(void) { wander_t = 0.f; }

void autopilot_drive(input_state_t *out, const player_t *p, float dt) {
    memset(out, 0, sizeof *out);
    wander_t += dt;

    float mx = 0.f, my = 0.f;
    float threat = 0.f;

    /* Steer away from anything lethal: enemies inside a personal bubble,
     * enemy bolts inside a tighter one (faster, so a stronger shove). */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive || enemies[i].spawn_t > 0.f) continue;
        float dx = p->x - enemies[i].x;
        float dy = p->y - enemies[i].y;
        float r  = 115.f + enemy_radius(&enemies[i]);
        float d2 = dx * dx + dy * dy;
        if (d2 >= r * r || d2 < 1.f) continue;
        float d = sqrtf(d2);
        float w = (r - d) / r;
        w *= w;
        mx += dx / d * w * 2.4f;
        my += dy / d * w * 2.4f;
        threat += w;
    }
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!ebullets[i].alive) continue;
        float dx = p->x - ebullets[i].x;
        float dy = p->y - ebullets[i].y;
        float d2 = dx * dx + dy * dy;
        if (d2 >= 75.f * 75.f || d2 < 1.f) continue;
        float d = sqrtf(d2);
        float w = (75.f - d) / 75.f;
        mx += dx / d * w * 3.2f;
        my += dy / d * w * 3.2f;
        threat += w;
    }

    /* Chase the nearest shard while things are calm — the multiplier is
     * what makes the attract run look competent (GDD 3.4). */
    if (threat < 0.7f) {
        int   best = -1;
        float bd2  = 160.f * 160.f;
        for (int i = 0; i < MAX_SHARDS; i++) {
            if (!shards[i].alive) continue;
            float dx = shards[i].x - p->x;
            float dy = shards[i].y - p->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bd2) { bd2 = d2; best = i; }
        }
        if (best >= 0 && bd2 > 1.f) {
            float d = sqrtf(bd2);
            mx += (shards[best].x - p->x) / d * 0.9f;
            my += (shards[best].y - p->y) / d * 0.9f;
        }
    }

    /* Keep off the walls, and drift along a slow Lissajous figure so the
     * ship roams the arena instead of camping the center. */
    const float WALL = 48.f;
    if (p->x < -ARENA_HALF_W + WALL) mx += (-ARENA_HALF_W + WALL - p->x) / WALL * 1.6f;
    if (p->x >  ARENA_HALF_W - WALL) mx -= (p->x - (ARENA_HALF_W - WALL)) / WALL * 1.6f;
    if (p->y < -ARENA_HALF_H + WALL) my += (-ARENA_HALF_H + WALL - p->y) / WALL * 1.6f;
    if (p->y >  ARENA_HALF_H - WALL) my -= (p->y - (ARENA_HALF_H - WALL)) / WALL * 1.6f;

    float tx = fm_cosf(wander_t * 0.31f) * (ARENA_HALF_W * 0.45f);
    float ty = fm_sinf(wander_t * 0.23f) * (ARENA_HALF_H * 0.45f);
    mx += (tx - p->x) * 0.004f;
    my += (ty - p->y) * 0.004f;

    float m2 = mx * mx + my * my;
    if (m2 > 1.f) {
        float inv = 1.f / sqrtf(m2);
        mx *= inv;
        my *= inv;
    }
    out->move_x = mx;
    out->move_y = my;

    /* Aim at the nearest enemy a shot can actually hurt (snake bodies
     * soak bullets, so this naturally tracks the tail), snapped to the
     * 8-way C-button grid that projectiles_fire_tick expects. */
    int   tgt = -1;
    float td2 = 1e12f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive || enemies[i].spawn_t > 0.f) continue;
        if (!enemy_vulnerable(i)) continue;
        float dx = enemies[i].x - p->x;
        float dy = enemies[i].y - p->y;
        float d2 = dx * dx + dy * dy;
        if (d2 < td2) { td2 = d2; tgt = i; }
    }
    if (tgt >= 0) {
        float dx = enemies[tgt].x - p->x;
        float dy = enemies[tgt].y - p->y;
        float ax = fabsf(dx), ay = fabsf(dy);
        out->c_right = dx > 0.f && ax > ay * SECTOR_TAN;
        out->c_left  = dx < 0.f && ax > ay * SECTOR_TAN;
        out->c_up    = dy > 0.f && ay > ax * SECTOR_TAN;
        out->c_down  = dy < 0.f && ay > ax * SECTOR_TAN;
    }

    /* Swamped and fully charged: bomb (main.c edge-detects Z, GDD 3.5). */
    if (threat > 2.4f && bomb_charge() >= 1.f)
        out->btn_z = true;
}
