# VOIDSTRIDER64

A procedurally-generated twin-stick arena shooter for Nintendo 64: Geometry Wars'
arena combat staged inside a Space Giraffe wormhole tunnel.
Stack: **libdragon (preview branch)** + **tiny3d** + **rdpq** + libdragon mixer, in C.

Full design spec (read when you need design context): @VOIDSTRIDER64_GDD.md

---

## Non-negotiable constraints

1. **Everything is generated, nothing is stored — except the music.** Meshes, textures,
   palettes, and SFX come from code (parametric generators, HSV wheel walks, a software
   synth). **Never** add model importers or sampled SFX. The one deliberate exception is
   music (GDD 7.3, revised 2026-07-03): the **title/options** theme is a hand-composed
   **.xm** tracker module (`assets/music/title.xm` → .xm64 via `audioconv64`, played by
   `xm64player`); the **gameplay/pause** soundtrack is `assets/music/soundtrack.mp3`,
   decoded to WAV by host ffmpeg in `build.sh` and converted to VADPCM **.wav64**
   streamed from ROM. A second contained exception (2026-07-04): the **boot splash**
   (`src/render/splash.c`) uses two authored logo sprites and two authored sounds from
   `assets/splash/` (host ffmpeg downscales/decodes in `build.sh`, then
   mksprite/audioconv64). Keep exceptions contained to music + boot splash — never
   gameplay SFX or gameplay graphics.
2. **8 MB / Expansion Pak is mandatory.** `main.c` calls `assert_memory_expanded()` at
   boot. Never add a 4 MB fallback path (GDD 2).
3. **libdragon preview branch is required** (tiny3d depends on it). Submodules are pinned
   to exact SHAs; upgrade deliberately at milestone boundaries only, never casually.
4. **Readability first.** Background (tunnel) stays lower saturation/value than the
   gameplay layer at all times; the grid/ship layer is never rolled with the tunnel.
5. **60 fps at 320×240.** Visual richness degrades under load (frame-budget director,
   GDD 9.2); gameplay never does.

---

## Build & run

No host toolchain and no `libdragon` CLI — raw Docker via the wrapper script:

```bash
sudo bash build.sh        # -> voidstrider64.z64 (docker needs sudo on this machine)
```

The script installs libdragon (submodule) into the container, builds tiny3d, then runs
`make`. Full output is tee'd to `build.log`.

- **libdragon submodule carries required uncommitted patches** (`include/fgeom.h`,
  `include/rspq_profile.h`, patched `include/libdragon.h` + `Makefile`, `src/fgeom.c`)
  backported for tiny3d. If the submodule is ever re-inited, recopy them from
  `/home/ahscott/Projects/n64/libdragon/`.
- IDE clang errors about missing `libdragon.h`/`t3d/*.h` are expected and harmless —
  headers only resolve inside the Docker container.
- Test in **Ares** (Homebrew mode ON); verify milestones on real hardware.

---

## Project layout (GDD 9.4)

```
src/main.c      boot (Pak check), main loop, state machine
src/input/      controller polling, control-scheme mapping (stick+C-aim / analog-aim)
src/sim/        player, enemies, projectiles, collision, spawn director
src/gen/        PROCEDURAL generation: mesh_gen, palette_gen, tunnel_gen, grid_sim
src/audio/      code synth (osc/ADSR/filters) for SFX + .xm64 music playback
src/render/     tiny3d passes: tunnel, grid, entities, particles, UI
src/meta/       scoring, EEPROM saves, accessibility options
```

---

## Architecture invariants

- **Tunnel is a conveyor belt** (GDD 6.1): fixed ring window, nearest ring recycles to
  the far end; the camera never moves — tunnel data scrolls toward it.
- **Fixed 60 Hz logic step** decoupled from render; sim stays deterministic per seed
  (cosmetic seed and difficulty seed are separate RNG streams, GDD 8.3).
- **No `malloc` in the hot loop.** Pools allocated at boot with `malloc_uncached` for
  anything the RSP DMAs. Per-frame-rewritten buffers (tunnel verts, matrices) are
  double-buffered — the RSP may still read frame N-1 while the CPU writes frame N.
- **rspq blocks bake addresses, not contents** — record draw blocks once at init,
  rewrite the underlying vert/matrix buffers per frame.
- **T3DVertPacked gotchas:** `normA`/`normB` is one packed `uint16_t`
  (`t3d_vert_pack_normal()`), positions are `int16_t`. No `t3d_frame_end()` exists;
  frame is `rdpq_attach` → `t3d_frame_start` → draws → `rdpq_detach_show`.

---

## Code conventions

- **C (C11)**, libdragon style: `snake_case`, `UPPER_SNAKE_CASE` constants, `_t` typedefs.
- Use `fm_sinf`/`fm_cosf` fast math in per-frame code, not libm `sinf`.
- Comments explain **why** (and cite GDD sections), not what.

## Never do

- No asset importers, no sampled SFX, no pre-baked textures. (Music + boot-splash
  assets only.)
- No 4 MB fallback; don't weaken the Expansion Pak boot check.
- No submodule SHA bumps outside a deliberate, tested upgrade.
- No per-frame heap allocation.
- Don't roll/deform the gameplay layer with the tunnel — controls stay screen-relative.
