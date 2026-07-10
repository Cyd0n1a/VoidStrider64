/*
          _       _        _          _                  _                _                    _             _        
        /\ \     /\ \     /\_\       /\ \               /\ \             /\ \   _           /\ \          / /\      
       /  \ \    \ \ \   / / /      /  \ \____         /  \ \           /  \ \   /\_\         \ \ \        / /  \     
      / /\ \ \    \ \ \_/ / /      / /\ \_____\       / /\ \ \         / /\ \ \_/ / /         /\ \_\      / / /\ \__  
     / / /\ \ \    \ \___/ /      / / /\/___  /      / / /\ \ \       / / /\ \___/ /         / /\/_/     / / /\ \___\ 
    / / /  \ \_\    \ \ \_/      / / /   / / /      / / /  \ \_\     / / /  \/____/         / / /        \ \ \ \/___/ 
   / / /    \/_/     \ \ \      / / /   / / /      / / /   / / /    / / /    / / /         / / /          \ \ \       
  / / /               \ \ \    / / /   / / /      / / /   / / /    / / /    / / /         / / /       _    \ \ \      
 / / /________         \ \ \   \ \ \__/ / /      / / /___/ / /    / / /    / / /      ___/ / /__     /_/\__/ / /      
/ / /_________\         \ \_\   \ \___\/ /      / / /____\/ /    / / /    / / /      /\__\/_/___\    \ \/___/ /       
\/____________/          \/_/    \/_____/       \/_________/     \/_/     \/_/       \/_________/     \_____\/        
                                                                                                                       
*/

#include "powerups.h"
#include <math.h>
#include <libdragon.h>

powerup_t powerups[MAX_POWERUPS];
powerup_state_t powerup_state;

/* Spawn probability weights (out of 100) */
#define PU_SPAWN_CHANCE_BASE 8  /* base 8% scaled by multiplier */
#define PU_WEIGHT_DRONE      15
#define PU_WEIGHT_SHIELD     20
#define PU_WEIGHT_SLOWMO     18
#define PU_WEIGHT_SCORE_MULT 13
#define PU_WEIGHT_MAGNET     18
#define PU_WEIGHT_FIRERATE   16
/* Total = 100 */

void powerups_init(void) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerups[i].alive = false;
    }
    powerup_state.drone_count = 0;
    powerup_state.drone_fire_cooldown = 0.f;
    powerup_state.shield_active = 0.f;
    powerup_state.slowmo_active = 0.f;
    powerup_state.score_mult_bonus_timer = 0.f;
    powerup_state.magnet_active = 0.f;
    powerup_state.firerate_boost_timer = 0.f;
}

void powerups_update(float dt) {
    /* Update active powerup timers */
    if (powerup_state.shield_active > 0.f) {
        powerup_state.shield_active -= dt;
    }
    if (powerup_state.slowmo_active > 0.f) {
        powerup_state.slowmo_active -= dt;
    }
    if (powerup_state.score_mult_bonus_timer > 0.f) {
        powerup_state.score_mult_bonus_timer -= dt;
    }
    if (powerup_state.magnet_active > 0.f) {
        powerup_state.magnet_active -= dt;
    }
    if (powerup_state.firerate_boost_timer > 0.f) {
        powerup_state.firerate_boost_timer -= dt;
    }

    /* Drone fire cooldown */
    if (powerup_state.drone_fire_cooldown > 0.f) {
        powerup_state.drone_fire_cooldown -= dt;
    }

    /* Update all active powerups: spin, drift, despawn */
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerups[i].alive) continue;

        powerups[i].spin += 3.5f * dt;  /* rotation speed */
        
        /* Slow descent down the tunnel */
        powerups[i].y += 40.f * dt;
        powerups[i].x += powerups[i].vx * dt;
        
        /* Despawn if off-screen or timer expired */
        powerups[i].timer -= dt;
        if (powerups[i].timer <= 0.f || powerups[i].y > 320.f) {
            powerups[i].alive = false;
        }
    }
}

void powerups_spawn(float x, float y, powerup_type_t type) {
    /* Find first available slot */
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (powerups[i].alive) continue;

        powerups[i].alive = true;
        powerups[i].type = type;
        powerups[i].x = x;
        powerups[i].y = y;
        powerups[i].vx = (rand_f32() - 0.5f) * 40.f;  /* small lateral drift */
        powerups[i].vy = 0.f;
        powerups[i].timer = 15.f;  /* 15 second despawn timer */
        powerups[i].spin = 0.f;
        powerups[i].gen = rand_u32();
        return;
    }
}

void powerup_apply(powerup_type_t type) {
    switch (type) {
    case PU_DRONE:
        /* Add drone, max 3 per player */
        if (powerup_state.drone_count < 3) {
            powerup_state.drone_angles[powerup_state.drone_count] = 
                (float)powerup_state.drone_count * (2.0f * M_PI / 3.0f);
            powerup_state.drone_count++;
        }
        powerup_state.drone_fire_cooldown = 0.f;  /* reset cooldown on pickup */
        break;

    case PU_SHIELD:
        /* Restore shield charge; doesn't stack if already active */
        if (powerup_state.shield_active <= 0.f) {
            powerup_state.shield_active = 12.f;  /* 12 second regeneration window */
        }
        break;

    case PU_SLOWMO:
        powerup_state.slowmo_active = 6.f;
        break;

    case PU_SCORE_MULT:
        powerup_state.score_mult_bonus_timer = 10.f;
        break;

    case PU_MAGNET:
        powerup_state.magnet_active = 12.f;
        break;

    case PU_FIRERATE:
        powerup_state.firerate_boost_timer = 8.f;
        break;

    default:
        break;
    }
}

void powerups_clear(void) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerups[i].alive = false;
    }
}

int powerups_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (powerups[i].alive) count++;
    }
    return count;
}

/* Query functions for sim and render */
float powerups_get_slow_factor(void) {
    return (powerup_state.slowmo_active > 0.f) ? 0.5f : 1.0f;
}

bool powerups_shield_active(void) {
    return powerup_state.shield_active > 0.f;
}

bool powerups_magnet_active(void) {
    return powerup_state.magnet_active > 0.f;
}

bool powerups_firerate_boosted(void) {
    return powerup_state.firerate_boost_timer > 0.f;
}

int powerups_score_mult_bonus(void) {
    /* +50% multiplier while active, rounds down */
    if (powerup_state.score_mult_bonus_timer > 0.f) {
        return 1;  /* signals +50% boost */
    }
    return 0;
}
