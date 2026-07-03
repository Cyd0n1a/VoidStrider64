#include "bomb.h"

#define FILL_PER_SEC   (1.f / 40.f)
#define FILL_PER_SHARD 0.025f

static float charge;

void bomb_init(void)   { charge = 1.f; }
float bomb_charge(void) { return charge; }

void bomb_update(float dt) {
    charge += FILL_PER_SEC * dt;
    if (charge > 1.f) charge = 1.f;
}

void bomb_notify_shard(void) {
    charge += FILL_PER_SHARD;
    if (charge > 1.f) charge = 1.f;
}

bool bomb_try_fire(void) {
    if (charge < 1.f) return false;
    charge = 0.f;
    return true;
}
