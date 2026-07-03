#include <libdragon.h>
#include <t3d/t3d.h>
#include "input/input.h"
#include "render/render.h"
#include "gen/tunnel_gen.h"
#include "gen/grid_sim.h"
#include "sim/player.h"
#include "sim/enemies.h"
#include "sim/projectiles.h"
#include "sim/shards.h"
#include "sim/director.h"
#include "meta/scoring.h"
#include "audio/synth.h"

/* Cosmetic vs difficulty seeds are separate RNG streams (GDD 8.3);
 * fixed for now, player-enterable in M5. */
#define COSMETIC_SEED   0xC0FFEE64u
#define DIFFICULTY_SEED (0xC0FFEE64u ^ 0x9E3779B9u)

#define START_LIVES   3
#define RESPAWN_IFRAMES 2.5f

typedef enum { ST_PLAY, ST_GAMEOVER } game_state_t;

static player_t     player;
static game_state_t state;
static int          lives;
static float        invuln;

static void run_reset(void) {
    player_init(&player);
    enemies_init(DIFFICULTY_SEED);
    projectiles_init();
    shards_init();
    director_init(DIFFICULTY_SEED ^ 0xA5A5A5A5u);
    scoring_init();
    grid_init();
    lives  = START_LIVES;
    invuln = 0.f;
    state  = ST_PLAY;
}

static void player_hit(void) {
    synth_player_die();
    grid_impulse(player.x, player.y, 90.f, 150.f);
    scoring_death();       /* multiplier streak resets (GDD 8.1) */

    /* Classic screen-clear on death, then respawn in place with brief
     * invulnerability (GDD 3.6). */
    enemies_clear();
    projectiles_clear();
    shards_clear();

    lives--;
    if (lives < 0)
        state = ST_GAMEOVER;
    else
        invuln = RESPAWN_IFRAMES;
}

static void sim_step(float dt) {
    input_poll();
    const input_state_t *inp = input_get();

    if (state == ST_GAMEOVER) {
        if (inp->btn_start) run_reset();
        tunnel_update(dt, 0.15f);
        grid_update(dt);
        return;
    }

    player_update(&player, inp, dt);
    projectiles_fire_tick(inp, &player, dt);
    projectiles_update(dt);
    director_update(dt, player.x, player.y);
    enemies_update(dt);

    /* Bullet x enemy: circle checks; counts are small enough that the
     * broad-phase grid can wait (GDD 9.1 collision, M2 scope). */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].alive) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].alive) continue;
            float dx = enemies[e].x - bullets[b].x;
            float dy = enemies[e].y - bullets[b].y;
            float rr = enemy_radius(&enemies[e]) + 4.f;
            if (dx * dx + dy * dy < rr * rr) {
                bullets[b].alive = false;
                float ex = enemies[e].x, ey = enemies[e].y;
                int   gen = enemies[e].gen;
                enemies_kill(e);
                shards_burst(ex, ey, gen == 0 ? 3 : 2);
                break;
            }
        }
    }

    /* Enemy x player (skip during i-frames and spawn grace). */
    if (invuln > 0.f) {
        invuln -= dt;
    } else {
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].alive || enemies[e].spawn_t > 0.f) continue;
            float dx = enemies[e].x - player.x;
            float dy = enemies[e].y - player.y;
            float rr = enemy_radius(&enemies[e]) + 9.f;
            if (dx * dx + dy * dy < rr * rr) {
                player_hit();
                break;
            }
        }
    }

    shards_update(dt, player.x, player.y);
    scoring_update(dt);

    /* Ship's wake dents the membrane as it skates (GDD 6.3). */
    grid_impulse(player.x, player.y,
                 130.f * dt * (0.25f + 0.75f * player.speed_norm), 42.f);

    /* Tunnel reacts to combat intensity (GDD 6.2). */
    tunnel_update(dt, director_intensity());
    grid_update(dt);
}

int main(void) {
    /* GDD 2: Expansion Pak is a hard requirement. libdragon shows an
     * error screen and halts if the full 8MB isn't present. */
    assert_memory_expanded();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    timer_init();

    input_init();
    synth_init();
    t3d_init((T3DInitParams){});
    render_init();
    tunnel_init(COSMETIC_SEED);
    run_reset();

    /* Fixed 60 Hz sim step decoupled from render (GDD 9.1): keeps the
     * sim deterministic per seed regardless of frame time. */
    #define STEP_US 16667
    long long prev  = timer_ticks();
    long long accum = 0;
    float     total = 0.f;

    while (1) {
        long long now  = timer_ticks();
        long long diff = TIMER_MICROS_LL(now - prev);
        prev = now;
        if (diff > 100000) diff = 100000;
        accum += diff;
        total += (float)diff / 1000000.f;

        while (accum >= STEP_US) {
            accum -= STEP_US;
            sim_step(STEP_US / 1000000.f);
        }

        synth_poll();

        hud_state_t hud = {
            .score    = scoring_score(),
            .mult     = scoring_mult(),
            .lives    = lives,
            .gameover = (state == ST_GAMEOVER),
            .invuln   = invuln,
        };
        surface_t *disp = display_get();
        render_frame(disp, total, &player, &hud);
    }

    return 0;
}
