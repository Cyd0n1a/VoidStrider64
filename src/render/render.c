#include "render.h"
#include "../gen/tunnel_gen.h"
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <libdragon.h>

static T3DViewport viewport;
static surface_t   zbuf;

/* Double-buffered: the RSP may still be DMA-ing frame N-1's verts/matrix
 * while the CPU writes frame N, so alternate between two sets. Each rspq
 * block bakes in its buffer's addresses; contents are re-read every run. */
static T3DVertPacked *tunnel_verts[2];
static T3DMat4FP     *tunnel_mat[2];
static rspq_block_t  *tunnel_dpl[2];
static int            frame_idx;

static rspq_block_t *record_tunnel_dpl(T3DVertPacked *verts, T3DMat4FP *mat) {
    rspq_block_t *dpl;
    rspq_block_begin();
        t3d_matrix_push(mat);
        for (int s = 0; s < TUNNEL_SEGS; s++) {
            /* Load ring s and ring s+1 (contiguous, slot-ordered buffer),
             * then stitch them with a quad per side. No cull flags are set,
             * so winding doesn't matter for the inside-viewed tube. */
            t3d_vert_load(verts + s * TUNNEL_PACKED_PER_RING, 0, TUNNEL_SIDES * 2);
            for (int j = 0; j < TUNNEL_SIDES; j++) {
                int a = j;
                int b = (j + 1) % TUNNEL_SIDES;
                int c = TUNNEL_SIDES + j;
                int d = TUNNEL_SIDES + (j + 1) % TUNNEL_SIDES;
                t3d_tri_draw(a, c, b);
                t3d_tri_draw(b, c, d);
            }
            t3d_tri_sync();
        }
        t3d_matrix_pop(1);
    dpl = rspq_block_end();
    return dpl;
}

void render_init(void) {
    viewport = t3d_viewport_create();
    zbuf     = surface_alloc(FMT_RGBA16, 320, 240);

    for (int i = 0; i < 2; i++) {
        tunnel_verts[i] = malloc_uncached(sizeof(T3DVertPacked) * TUNNEL_PACKED_COUNT);
        tunnel_mat[i]   = malloc_uncached(sizeof(T3DMat4FP));
        t3d_mat4fp_identity(tunnel_mat[i]);
        tunnel_dpl[i]   = record_tunnel_dpl(tunnel_verts[i], tunnel_mat[i]);
    }
    frame_idx = 0;

    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO,
                            rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));
}

void render_frame(surface_t *disp, float time) {
    frame_idx ^= 1;

    tunnel_build_verts(tunnel_verts[frame_idx], time);

    /* Chase-cam roll: rotate the tunnel around the view axis; the camera
     * (and later the grid/gameplay layer) stays fixed (GDD 6.2). */
    float r        = tunnel_roll();
    float quat[4]  = { 0.f, 0.f, fm_sinf(r * 0.5f), fm_cosf(r * 0.5f) };
    float scale[3] = { 1.f, 1.f, 1.f };
    float pos[3]   = { 0.f, 0.f, 0.f };
    t3d_mat4fp_from_srt(tunnel_mat[frame_idx], scale, quat, pos);

    T3DVec3 cam_pos = {{ 0.f, 0.f,    0.f }};
    T3DVec3 cam_tgt = {{ 0.f, 0.f, -100.f }};
    T3DVec3 up      = {{ 0.f, 1.f,    0.f }};
    t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(78.f), 8.f, 1900.f);
    t3d_viewport_look_at(&viewport, &cam_pos, &cam_tgt, &up);

    rdpq_attach(disp, &zbuf);
    t3d_frame_start();
    t3d_viewport_attach(&viewport);
    t3d_screen_clear_color(RGBA32(3, 2, 8, 255));
    t3d_screen_clear_depth();

    /* Tunnel pass: pure vertex color, self-illuminated (GDD 9.1). */
    t3d_light_set_count(0);
    t3d_state_set_drawflags(T3D_FLAG_SHADED | T3D_FLAG_DEPTH | T3D_FLAG_NO_LIGHT);
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rspq_block_run(tunnel_dpl[frame_idx]);

    /* M0 headroom counter (GDD 10, M0: "confirm framerate headroom"). */
    rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 16, 20,
                     "FPS %.1f", display_get_fps());

    rdpq_detach_show();
}
