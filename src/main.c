#include <libdragon.h>
#include <t3d/t3d.h>
#include "input/input.h"
#include "render/render.h"
#include "gen/tunnel_gen.h"

/* Cosmetic seed (GDD 8.3) — fixed for the M0 spike, player-enterable later. */
#define M0_SEED 0xC0FFEE64u

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
    tunnel_init(M0_SEED);

    long long prev  = timer_ticks();
    float     total = 0.f;

    while (1) {
        long long now = timer_ticks();
        float dt = (float)TIMER_MICROS_LL(now - prev) / 1000000.f;
        prev = now;
        if (dt > 0.1f) dt = 0.1f;
        total += dt;

        input_poll();
        const input_state_t *inp = input_get();

        /* M0 test rig: hold A to simulate max combat intensity and watch
         * the tunnel speed up, churn, and roll harder (GDD 6.2). The real
         * intensity metric arrives with the spawn director (M3). */
        float intensity = inp->btn_a ? 1.f : 0.f;
        tunnel_update(dt, intensity);

        surface_t *disp = display_get();
        render_frame(disp, total);
    }

    return 0;
}
