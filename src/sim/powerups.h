/*
          _       _        _          _                  _                _                    _             _        
        /\ \     /\ \     /\_\       /\ \               /\ \             /\ \     _           /\ \          / /\      
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

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "player.h"

#define MAX_POWERUPS 12

/* Powerup types with distinct gameplay effects */
typedef enum {
    PU_DRONE,           /* orbiting satellite that auto-shoots enemies */
    PU_SHIELD,          /* absorbs one hit */
    PU_SLOWMO,          /* enemies + bullets 50% speed for 6 seconds */
    PU_SCORE_MULT,      /* +50% score multiplier for 10 seconds */
    PU_MAGNET,          /* auto-collect shards in 80px radius for 12 seconds */
    PU_FIRERATE,        /* boosts fire rate for 8 seconds */
    POWERUP_TYPE_COUNT
} powerup_type_t;

typedef struct {
    bool           alive;
    powerup_type_t type;
    float          x, y;
    float          vx, vy;         /* slow descent/drift down tunnel */
    float          timer;          /* time-to-live; despawns at 0 */
    float          spin;           /* rotation for rendering */
    int            gen;            /* procedural variant seed */
} powerup_t;

extern powerup_t powerups[MAX_POWERUPS];

/* Powerup state tracking */
typedef struct {
    int   drone_count;
    float drone_angles[3];          /* orbital angles for up to 3 drones */
    float drone_fire_cooldown;
    float shield_active;            /* timer; shield absorbs next hit if > 0 */
    float slowmo_active;            /* timer for 50% speed effect */
    float score_mult_bonus_timer;   /* timer for +50% multiplier bonus */
    float magnet_active;            /* timer for shard auto-collect in 80px */
    float firerate_boost_timer;     /* timer for faster fire rate */
} powerup_state_t;

extern powerup_state_t powerup_state;

void powerups_init(void);
void powerups_update(float dt);
void powerups_spawn(float x, float y, powerup_type_t type);
void powerup_apply(powerup_type_t type);
void powerups_clear(void);
int  powerups_count(void);

/* Query active powerup state for simulation/render */
float powerups_get_slow_factor(void);
bool  powerups_shield_active(void);
bool  powerups_magnet_active(void);
bool  powerups_firerate_boosted(void);
int   powerups_score_mult_bonus(void);

