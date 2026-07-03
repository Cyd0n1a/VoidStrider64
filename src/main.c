#include <libdragon.h>
#include <t3d/t3d.h>
#include "input/input.h"
#include "render/render.h"
#include "render/render_entities.h"
#include "gen/tunnel_gen.h"
#include "gen/grid_sim.h"
#include "sim/player.h"
#include "sim/enemies.h"
#include "sim/projectiles.h"
#include "sim/shards.h"
#include "sim/bomb.h"
#include "sim/director.h"
#include "meta/scoring.h"
#include "meta/options.h"
#include "meta/save.h"
#include "audio/synth.h"
#include "audio/music.h"

/* Default seeds; both player-editable on the seeds screen (GDD 8.3). */
#define DEFAULT_CSEED 0xC0FFEE64u
#define DEFAULT_DSEED (0xC0FFEE64u ^ 0x9E3779B9u)

#define START_LIVES     3
#define RESPAWN_IFRAMES 2.5f

static player_t player;
static screen_t state;
static int      lives;
static float    invuln;
static bool     z_prev;
static int      menu_cursor;
static uint32_t cseed = DEFAULT_CSEED;
static uint32_t dseed = DEFAULT_DSEED;
static float    run_secs;
static int      hs_rank = -1;

static void run_start(void) {
    /* Seeds apply from here: cosmetic reskins tunnel + bestiary,
     * difficulty drives behavior/spawn RNG streams (GDD 8.3). */
    tunnel_init(cseed);
    render_entities_reseed(cseed);
    player_init(&player);
    enemies_init(dseed);
    projectiles_init();
    shards_init();
    director_init(dseed ^ 0xA5A5A5A5u);
    scoring_init();
    bomb_init();
    grid_init();
    lives    = START_LIVES;
    invuln   = 0.f;
    z_prev   = false;
    run_secs = 0.f;
    hs_rank  = -1;
    state    = SCR_PLAY;
    music_gameplay();
}

static void player_hit(void) {
    synth_player_die();
    grid_impulse(player.x, player.y, 90.f * options_flash_scale(), 150.f);
    scoring_death();       /* multiplier streak resets (GDD 8.1) */

    /* Classic screen-clear on death, then respawn in place with brief
     * invulnerability (GDD 3.6). */
    enemies_clear();
    projectiles_clear();
    shards_clear();

    lives--;
    if (lives < 0) {
        state = SCR_GAMEOVER;
        music_stop();
        hs_rank = save_submit(scoring_score(), cseed, dseed,
                              (uint32_t)run_secs);
    } else {
        invuln = RESPAWN_IFRAMES;
    }
}

/* Edit one hex nibble of the seed pair; cursor 0..15 spans both rows. */
static void seed_nudge(int cursor, int delta) {
    uint32_t *s     = (cursor < 8) ? &cseed : &dseed;
    int       shift = (7 - (cursor % 8)) * 4;
    uint32_t  nib   = (*s >> shift) & 0xF;
    nib = (nib + (uint32_t)(16 + delta)) & 0xF;
    *s = (*s & ~(0xFu << shift)) | (nib << shift);
}

static void menu_step(const input_state_t *inp, float dt) {
    switch (state) {
    case SCR_TITLE:
        if (inp->btn_start) { run_start(); return; }
        if (inp->a_press)   { state = SCR_OPTIONS; menu_cursor = 0; }
        tunnel_update(dt, 0.25f + synth_beat_pulse() * 0.3f
                              * options_flash_scale());
        grid_update(dt);
        break;

    case SCR_OPTIONS:
        if (inp->d_up   && menu_cursor > 0) menu_cursor--;
        if (inp->d_down && menu_cursor < 3) menu_cursor++;
        if (menu_cursor == 0) {
            if (inp->d_left)  g_options.bg_intensity -= 0.1f;
            if (inp->d_right) g_options.bg_intensity += 0.1f;
            if (g_options.bg_intensity < 0.2f) g_options.bg_intensity = 0.2f;
            if (g_options.bg_intensity > 1.f)  g_options.bg_intensity = 1.f;
        }
        if (menu_cursor == 1 && (inp->d_left || inp->d_right || inp->a_press))
            g_options.reduce_flash = !g_options.reduce_flash;
        if (menu_cursor == 2 && inp->a_press) {
            state = SCR_SEEDS;
            menu_cursor = 0;
            break;
        }
        if (inp->b_press || (menu_cursor == 3 && inp->a_press)) {
            save_options_sync();
            state = SCR_TITLE;
            menu_cursor = 0;
        }
        tunnel_update(dt, 0.2f);
        grid_update(dt);
        break;

    case SCR_SEEDS:
        if (inp->d_left)  menu_cursor = (menu_cursor + 15) % 16;
        if (inp->d_right) menu_cursor = (menu_cursor + 1) % 16;
        if (inp->d_up)    seed_nudge(menu_cursor, +1);
        if (inp->d_down)  seed_nudge(menu_cursor, -1);
        if (inp->b_press) { state = SCR_OPTIONS; menu_cursor = 2; }
        tunnel_update(dt, 0.2f);
        grid_update(dt);
        break;

    case SCR_GAMEOVER:
        if (inp->btn_start) {
            state = SCR_TITLE;
            music_title();
        }
        tunnel_update(dt, 0.15f);
        grid_update(dt);
        break;

    case SCR_PAUSE:
        /* GDD 4: D-pad adjusts background intensity mid-pause. The
         * soundtrack keeps playing (music design, 2026-07-03). */
        if (inp->d_left)  g_options.bg_intensity -= 0.1f;
        if (inp->d_right) g_options.bg_intensity += 0.1f;
        if (g_options.bg_intensity < 0.2f) g_options.bg_intensity = 0.2f;
        if (g_options.bg_intensity > 1.f)  g_options.bg_intensity = 1.f;
        if (inp->btn_start) {
            save_options_sync();
            state = SCR_PLAY;
        }
        break;

    default: break;
    }
}

static void sim_step(float dt) {
    input_poll();
    const input_state_t *inp = input_get();

    if (state != SCR_PLAY) {
        menu_step(inp, dt);
        return;
    }

    if (inp->btn_start) {
        state = SCR_PAUSE;
        return;
    }

    run_secs += dt;

    player_update(&player, inp, dt);
    projectiles_fire_tick(inp, &player, dt);
    projectiles_update(dt);
    director_update(dt, player.x, player.y);
    enemies_update(dt, player.x, player.y, player.vx, player.vy);
    bomb_update(dt);

    /* Smart bomb (GDD 3.5/8.2): Z detonates a full charge — clears the
     * screen and enemy bolts (no splits, kills still score) and grants
     * brief invulnerability. */
    if (inp->btn_z && !z_prev && bomb_try_fire()) {
        for (int e = 0; e < MAX_ENEMIES; e++)
            if (enemies[e].alive)
                enemies_kill(e, false);
        for (int b = 0; b < MAX_EBULLETS; b++)
            ebullets[b].alive = false;
        synth_bomb();
        grid_impulse(player.x, player.y, 130.f * options_flash_scale(), 330.f);
        if (invuln < 1.f) invuln = 1.f;
    }
    z_prev = inp->btn_z;

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
                /* Armored snake segments soak the shot (tail-first
                 * destruction, GDD 3.1). */
                if (enemy_vulnerable(e)) {
                    float ex = enemies[e].x, ey = enemies[e].y;
                    int   ns = enemy_shard_count(&enemies[e]);
                    enemies_kill(e, true);
                    shards_burst(ex, ey, ns);
                }
                break;
            }
        }
    }

    /* Enemy / enemy-bolt x player (skip during i-frames; enemies also
     * respect their spawn grace). */
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
    if (invuln <= 0.f && state == SCR_PLAY) {
        for (int b = 0; b < MAX_EBULLETS; b++) {
            if (!ebullets[b].alive) continue;
            float dx = ebullets[b].x - player.x;
            float dy = ebullets[b].y - player.y;
            if (dx * dx + dy * dy < 13.f * 13.f) {
                ebullets[b].alive = false;
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

    /* Tunnel reacts to combat intensity, with the music's beats layered
     * on top for the audio-reactive Space Giraffe feel (GDD 6.2). */
    float inten = director_intensity()
                + synth_beat_pulse() * 0.15f * options_flash_scale();
    tunnel_update(dt, inten > 1.f ? 1.f : inten);
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

    dfs_init(DFS_DEFAULT_LOCATION);
    input_init();
    options_init();
    save_init();          /* loads high scores + persisted options */
    synth_init();
    music_init();
    t3d_init((T3DInitParams){});
    render_init();
    tunnel_init(cseed);

    state = SCR_TITLE;
    music_title();

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
            .screen   = state,
            .score    = scoring_score(),
            .mult     = scoring_mult(),
            .lives    = lives,
            .invuln   = invuln,
            .bomb     = bomb_charge(),
            .cursor   = menu_cursor,
            .cseed    = cseed,
            .dseed    = dseed,
            .hi_score = save_hi_score(),
            .hs_rank  = hs_rank,
            .run_secs = (uint32_t)run_secs,
            .save_ok  = save_available(),
        };
        surface_t *disp = display_get();
        render_frame(disp, total, &player, &hud);
    }

    return 0;
}
