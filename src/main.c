#include <libdragon.h>
#include <t3d/t3d.h>
#include "input/input.h"
#include "render/render.h"
#include "gen/tunnel_gen.h"
#include "gen/grid_sim.h"
#include "sim/player.h"

/* Cosmetic seed (GDD 8.3) — fixed for now, player-enterable later (M5). */
#define RUN_SEED 0xC0FFEE64u

int main(void) {
    /* GDD 2: Expansion Pak is a hard requirement. libdragon shows an
     * error screen and halts if the full 8MB isn't present. */
    assert_memory_expanded();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    timer_init();

    input_init();
    t3d_init((T3DInitParams){});
    render_init();
    tunnel_init(RUN_SEED);
    grid_init();

    player_t player;
    player_init(&player);

    /* Fixed 60 Hz sim step decoupled from render (GDD 9.1): keeps the
     * sim deterministic per seed regardless of frame time. */
    #define STEP_US 16667
    long long prev   = timer_ticks();
    long long accum  = 0;
    float     total  = 0.f;
    bool      z_prev = false;

    while (1) {
        long long now  = timer_ticks();
        long long diff = TIMER_MICROS_LL(now - prev);
        prev = now;
        if (diff > 100000) diff = 100000;
        accum += diff;
        total += (float)diff / 1000000.f;

        while (accum >= STEP_US) {
            const float dt = STEP_US / 1000000.f;
            accum -= STEP_US;

            input_poll();
            const input_state_t *inp = input_get();

            player_update(&player, inp, dt);

            /* Ship's wake dents the membrane as it skates (GDD 6.3). */
            grid_impulse(player.x, player.y,
                         130.f * dt * (0.25f + 0.75f * player.speed_norm), 42.f);

            /* M1 test rig: Z fires a bomb-sized ripple at the ship, A
             * holds max tunnel intensity. Real triggers arrive M2/M3. */
            if (inp->btn_z && !z_prev)
                grid_impulse(player.x, player.y, 70.f, 95.f);
            z_prev = inp->btn_z;

            tunnel_update(dt, inp->btn_a ? 1.f : 0.f);
            grid_update(dt);
        }

        surface_t *disp = display_get();
        render_frame(disp, total, &player);
    }

    return 0;
}
