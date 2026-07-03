# VOIDSTRIDER64
### A Procedurally-Generated Twin-Stick Arena Shooter for Nintendo 64
**Game Design Document — v0.1**
**Engine:** libdragon (preview branch) + tiny3d
**Target Hardware:** Nintendo 64 with Expansion Pak (8MB RDRAM) — **mandatory**, not an optional boost.
**Authored by: (c) 2026 Amanda Hariette-Scott & Cydonis Heavy Industries. All Rights Reserved.
---

## 1. High Concept

Void Strider is a score-attack twin-stick shooter in the lineage of *Geometry Wars: Retro Evolved*, reimagined for N64 hardware with a *Space Giraffe*-inspired presentation twist: the player's rectangular arena is rendered as a semi-transparent grid **floating over an infinite, undulating wormhole tunnel** that the whole scene appears to travel through. Every visual asset (ship, enemies, particles, tunnel geometry, grid deformation, color palettes) and every sound *effect* (shots, explosions, pickups) is generated procedurally at runtime — there are no sampled SFX and no pre-baked texture art on disk. Music is the one deliberate exception: the score is hand-composed as **.xm tracker modules**, giving a human composer full melodic/harmonic control, and played back through libdragon's dedicated module player rather than being algorithmically generated. The game targets Expansion Pak–equipped hardware exclusively, and the ROM ships almost entirely as code, RSP microcode, small parameter tables, and a handful of .xm64 music files.

**One-sentence pitch:** *Geometry Wars' arena combat, staged inside a Space Giraffe wormhole, built almost entirely from procedural math (plus a hand-scored soundtrack) on Expansion-Pak-equipped N64 hardware.*

### 1.1 Pillars
1. **Everything is generated, nothing is stored — except the music.** Meshes, textures, palettes, and SFX waveforms come from formulas and small seeded RNG tables, not asset files. Music is authored conventionally as .xm tracker modules; it's the one deliberate, hand-crafted exception to this pillar, chosen because algorithmic composition can't yet match a human composer for melodic and harmonic intent.
2. **Depth as illusion, not geometry.** The playfield is a flat 2D combat plane; all sense of "traveling through a tunnel" comes from the tiny3d background layer moving underneath/around a transparent grid.
3. **Readability first.** Bright, high-contrast vector-style enemies and player ship must always pop against the psychedelic backdrop — the background is deliberately kept lower-contrast and blurred/dimmed relative to the foreground.
4. **N64-native controls.** The control scheme is designed around the actual N64 controller (single analog stick + C-buttons + triggers), not a naive port of a dual-stick layout that doesn't exist on this pad.

---

## 2. Target Hardware & Engine Assumptions

- **Console:** NTSC/PAL Nintendo 64, RCP running at native clock.
- **RAM budget:** Expansion Pak presence is a **hard requirement**, not an opportunistic bonus — design directly against the full 8MB RDRAM as the baseline. Boot code should detect Pak absence and halt with a clear "Expansion Pak Required" message rather than attempting a degraded fallback mode; there is no bare-4MB code path to maintain.
- **SDK:** libdragon preview branch (required by tiny3d), GCC 14 toolchain, Newlib/C11.
- **Renderer:** tiny3d for all 3D work (wormhole tunnel, grid mesh, ship/enemy models, particles-as-billboards). tiny3d is a from-scratch RSP microcode + C API that DMAs vertex/matrix data to the RSP each draw call, layers directly on top of libdragon's `rdpq` API, and has no abstract "material" system — texture/lighting state is toggled explicitly per draw. This suits Void Strider well since every draw call already needs custom per-object parameters (color, deformation) rather than a shared material.
- **Audio:** libdragon's low-level mixer (`mixer.h`/`audio.h`) drives two parallel paths sharing one channel pool: (1) custom oscillator/envelope code writing synthesized PCM directly into mixer channels for all SFX, and (2) libdragon's built-in `xm64player` (based on the libxm player) streaming hand-composed **.xm** tracker music, converted to the N64-native **.xm64** format via the `audioconv64` tool, for the soundtrack. SFX stay fully procedural; music is the one sampled/authored exception.
- **Display target:** 320×240 (or 320×220 safe-area), 60Hz interlace-off ("progressive-ish" low-res mode common in N64 homebrew) to keep RDP fill-rate costs low and framerate high.
- **Test environment:** Ares emulator in Homebrew Mode for cycle-accurate iteration; validate milestone builds on real hardware via EverDrive64/SC64/64drive over USB.

---

## 3. Core Gameplay Loop

Standard "geometry-wars-like" loop, tuned for a single arena rather than multi-screen levels:

1. Player ship free-moves inside a bounded rectangular arena.
2. Player fires continuously in an aim direction.
3. Waves of geometric enemies spawn from the arena edges and pursue/weave toward the player using distinct movement patterns per enemy class.
4. Killing enemies drops **geometric shards** (small colored polygons) that must be collected to build the **score multiplier** (1×–10×), Geometry-Wars style — multiplier decays if the player stops collecting shards.
5. Occasional **bomb pickups / charge meter** clear the screen and grant brief invulnerability.
6. There is no fixed "end" — the game is endless survival, with difficulty (spawn rate, enemy variety, tunnel intensity) scaling continuously with elapsed time and score. A single life-total (typically 3) with respawn-in-place-with-brief-invulnerability governs game over.
7. On death, final score, longest multiplier streak, and survival time are shown; top scores persist in EEPROM/SRAM save.

### 3.1 Enemy Archetypes (initial set, all proc-gen meshes/behavior)
| Class | Silhouette (proc-gen) | Behavior | Color Family |
|---|---|---|---|
| Wanderer | Rotating tetrahedron | Drifts in slow noise-driven path, splits into 2 smaller ones on death | Cyan |
| Seeker | Elongated diamond | Direct pursuit of player with slight lead prediction | Magenta |
| Swarmer | Small triangle, spawns in packs of 6-10 | Boids-style flocking toward player | Yellow |
| Turret | Static hexagon | Doesn't move; fires slow aimed bolts | Orange |
| Pulsar | Pulsing icosahedron-ish cluster | Grows, then bursts into a ring of projectiles | Red |
| Snake (late-game) | Chain of linked tetrahedra | Follows a sinuous path, must be destroyed segment-by-segment tail-first | Green |

All meshes are built from a small library of procedurally-generated base primitives (see Section 5) with per-spawn seeded jitter so no two enemies of the same class look pixel-identical (slight vertex noise, rotation offsets, scale variance).

---

## 4. Controls (N64 Pad Adaptation)

The N64 controller has no second analog stick, so Void Strider adapts the classic twin-stick scheme rather than faking it:

**Primary scheme — "Stick + C-aim":**
- **Analog Stick:** Ship movement (full analog, 360°).
- **C-buttons (C-Up/Down/Left/Right):** 8-directional aim/fire in the combination directions (e.g., C-Up+C-Right = fire NE). Firing is continuous while any C-button is held; last-held direction persists like a "soft lock" so players can strafe while maintaining fire direction, easing the lack of stick precision.
- **Z-Trigger:** Smart bomb / screen clear (consumes charge meter).
- **R-Trigger:** Toggle **auto-aim-assist mode** for accessibility — aims at nearest threat automatically, fires with any C-button press regardless of direction. Off by default, purely optional.
- **A/B:** Confirm/back in menus; unused in core gameplay (reserved for a future secondary weapon).
- **D-Pad:** Adjusts background intensity / photosensitivity options mid-pause (see Section 8.4).

**Alternate scheme — "Analog-Aim":** For players who find C-button aiming awkward, an alternate mode maps aim to *stick angle + stick magnitude threshold* (small deflection = move only, full deflection = move + fire in stick direction), with C-buttons then repurposed as strafing nudges. Selectable in options; both schemes ship as first-class supported inputs, playtested equally.

---

## 5. Visual Design — Procedural Graphics Pipeline

### 5.1 Layer structure
Three tiny3d-rendered layers composited back-to-front every frame:

1. **Wormhole tunnel (background, far):** An open-ended cylindrical tube mesh the "camera" appears to fly through forever, per Section 6.
2. **Arena grid (midground, the "board"):** A flat, semi-transparent rectangular grid plane hovering in front of the tunnel — this is the actual collision playfield, rendered with alpha blending so the tunnel is visible *through* it, and with a **deformation ripple** shader-like effect (vertex displacement driven by recent explosions) reminiscent of Geometry Wars' bump-mapped floor grid.
3. **Foreground gameplay layer (ship, enemies, bullets, particles, shards):** Fully opaque, high-contrast, additively-blended where appropriate (muzzle flashes, explosions), always rendered last so it reads clearly over both background layers.

### 5.2 Procedural mesh generation
- A small set of **parametric primitive generators** run at boot (and periodically for variety) to build vertex/index buffers in RDRAM entirely from code: regular polygon extrusions, subdivided icosahedra, star-polygons, and Lissajous-curve-based rings.
- Enemy "species" are defined not as fixed meshes but as **generator functions** (base primitive + operation list: extrude, twist, spike, subdivide) parameterized by a per-run seed, so the whole enemy bestiary can be regenerated with a different look every session if desired (a "remix seed" is shown on the title screen, shareable like a Tetris 99 seed).
- The **player ship** is built from a small generator too (arrowhead/star hybrid) but with a fixed baseline seed for player recognizability, with only trailing thruster geometry procedurally animated.
- All meshes upload as `T3DVertPacked` buffers matching tiny3d's fixed interleaved vertex layout; static parts of a mesh are recorded once into a small tiny3d display list, while per-frame transforms (position, rotation, deformation) are written directly into the matrices/vertex buffers tiny3d re-DMAs each frame, which tiny3d is explicitly designed to support cheaply.

### 5.3 Procedural texturing & color
- No texture files. Small gradient/noise textures (e.g., 32×32 or 64×64 palettes) are synthesized once at boot into TMEM-friendly RGBA16/CI formats using simple value-noise and radial-gradient functions, then reused across many draw calls via the RDP.
- Enemy/tunnel/grid color palettes are generated from **HSV wheel walks**: a rotating base hue driven by elapsed survival time (the whole game's palette slowly drifts across the color wheel over a run, echoing Space Giraffe's shifting psychedelic tone), with each entity class offset by a fixed hue delta so classes stay distinguishable even as the palette rotates.
- Explosions and shard particles use simple procedurally-built radial gradient billboards (no photographic sprite sheets) rendered as camera-facing quads via tiny3d.

### 5.4 Performance-conscious rendering
- Tunnel and grid geometry use modest vertex counts (ring-based tunnel: ~16–24 sides × a rolling window of ~24–32 rings, scrolled via UV/position offset rather than regenerated each frame) to stay within RSP/RDP budgets at 60fps.
- Aggressive reliance on **additive blending + fog** for the tunnel depth cue instead of expensive per-pixel effects, since the RDP's blender and fog units are hardware-accelerated and cheap relative to shader-style tricks.
- A frame-budget director (Section 9) can dynamically cull tunnel ring density or particle counts if the frame is running long, prioritizing gameplay-layer readability and framerate over background fidelity.

---

## 6. The Wormhole & Grid Effect (Space Giraffe Homage)

This is the signature visual system, built specifically around tiny3d's strengths.

### 6.1 Tunnel construction
- The tunnel is a chain of **ring cross-sections** (not perfect circles — organic, LFO-perturbed polygons, closer to Space Giraffe's undulating tube than a plain cylinder) spaced along a virtual Z-axis "flight path."
- Each ring's radius and per-vertex offset are computed from a stack of summed sine/cosine terms with slowly-changing frequency/phase (a cheap substitute for full Perlin noise, well suited to the RSP's fixed-point math), producing continuous organic pulsing without per-frame CPU-side noise table lookups.
- Rings are recycled: as the nearest ring passes the camera it's popped and reinserted at the far end with freshly-computed parameters, giving an illusion of infinite travel without needing infinite geometry — this is the same "conveyor belt" trick used for procedural infinite-runner tunnels.
- Camera itself is static (or nearly so); the *tunnel data scrolls toward the camera*, which is far cheaper on this hardware than moving a real camera through a large generated world, and avoids any precision issues with large world coordinates.

### 6.2 Color & motion behavior
- Tunnel wall coloring cycles through the same HSV rotation driving enemy palettes, but at lower saturation/value so it recedes visually behind the gameplay layer.
- Tunnel "flight speed" and ring turbulence are tied to **game intensity** (current wave difficulty, recent player deaths, current multiplier) — calmer during lulls, more frantic and colorful during intense multi-enemy swarms or when a smart bomb detonates, giving the background a reactive, musical-visualizer-like quality akin to Space Giraffe's audio-reactive tunnel.
- A screen-space "chase-cam roll" — a slow, small rotation of the whole tunnel around the view axis — reinforces the sense of tumbling through a wormhole without disorienting actual gameplay (the grid/ship layer is deliberately *not* rolled, keeping player controls screen-relative and stable).

### 6.3 The transparent grid ("the board")
- The playfield is a flat plane subdivided into a grid of quads, rendered with per-vertex alpha (mostly transparent, brighter along grid lines) so the tunnel remains visible underneath.
- On explosions or heavy player movement, nearby grid vertices are displaced along their normal and springs back over a few frames (simple critically-damped spring integration per vertex neighborhood) — this is the "rippling floor" effect familiar from Geometry Wars, reinterpreted here as a semi-transparent membrane the player skates across while the wormhole flows beneath it.
- Grid line brightness pulses gently in time with the procedural music's beat clock (Section 7), reinforcing the sensation that the whole playfield is "riding" the tunnel rather than sitting statically in front of it.

---

## 7. Audio System — Procedural SFX + Tracker Music

Sound effects remain strictly procedural, synthesized into PCM in real time. **Music is the deliberate exception**: it's composed conventionally as .xm tracker modules rather than generated. Both paths run through libdragon's shared mixer, so the two are budgeted and voice-managed together (Section 7.3, Section 9.3).

### 7.1 Core synthesis primitives
A small software synth runs on the CPU (with careful attention to not starving RSP/RDP time), offering:
- **Oscillators:** square, saw, triangle, sine, and noise (LFSR-based), all generated as fixed-point sample loops.
- **Envelopes:** simple ADSR per voice for shots, explosions, and pickups.
- **Simple filters:** one-pole low/high-pass for shaping noise into "thump" (explosions) or "sizzle" (shard collection) sounds without needing FFT-based effects.
- **Voice mixer:** a fixed pool of mixer channels (e.g., 8–16 voices) covering SFX + music simultaneously, with priority-based voice-stealing when the pool is full during heavy combat.

### 7.2 Procedural SFX design
- **Player shot:** short square/saw blip with fast pitch downslide, pitch nudged per-shot by a tiny random offset so rapid fire doesn't sound mechanically identical.
- **Enemy death:** noise burst through a decaying low-pass filter, with burst duration and cutoff frequency scaled by the enemy class's "size," so bigger enemies sound bigger without needing unique authored sounds.
- **Shard pickup:** short rising sine blip, pitch stepped upward per consecutive pickup (mirrors the rising-pitch combo-feedback of Geometry Wars' collection sound) and reset on multiplier drop.
- **Smart bomb:** layered noise + descending sine sweep, all generated live, duration matched to the actual screen-clear animation length.
- All SFX parameters (pitch, duration, filter cutoff) are driven by small formulas keyed off gameplay state (enemy type, combo count, distance from player) rather than fixed clips, so audio "content" scales with the same seeds/state as the visuals.

### 7.3 Music (.xm tracker format — the non-procedural exception)
- The score is composed conventionally in a tracker (e.g., MilkyTracker or OpenMPT) as standard **.xm** modules, giving a human composer full control over melody, harmony, and arrangement. This is deliberately *not* algorithmically generated, unlike every other audio/visual system in the game.
- Source `.xm` files are converted at build time with libdragon's `audioconv64` tool into **.xm64**, a preprocessed/serialized format (ping-pong loops unrolled, patterns RLE-compressed, single-allocation loading) purpose-built for low-RAM streaming playback on N64.
- Playback uses libdragon's `xm64player_t` (built on the libxm player), which **streams instrument samples from ROM on demand** rather than preloading the whole module, and allocates one mixer channel per XM channel. Published libdragon figures put a 10-channel XM module at under 3% CPU and under 10% RSP, leaving plenty of headroom for the procedural SFX voices and the tiny3d rendering pipeline.
- Tracks are authored per game state (title/menu theme, base gameplay loop, one or more high-intensity variants) rather than continuously generated. Transitions between them are handled either by crossfading mixer channel volume across two simultaneously-playing modules, or by composing intensity tiers as separate pattern ranges within a single module and jumping between them as the Section 9.2 intensity metric rises and falls.
- `xm64player_set_effect_callback` lets the composer place **custom sync-cue effects** directly into the tracker patterns (an otherwise-unused XM effect letter, authored alongside the notes). A cue on a downbeat can, for example, nudge the tunnel's HSV rotation speed or trigger a beat-synced grid pulse (Section 6.3) — so the hand-authored music and the procedural visuals still feel like one reactive system, even though only one half of that pairing is actually generated at runtime.
- Because samples stream from ROM rather than sitting fully resident in RAM, music's RAM footprint stays modest and roughly constant regardless of track length — important given the SFX synth, tiny3d buffers, and gameplay state are already sharing the same 8MB budget (Section 9.3).

---

## 8. Meta Systems

### 8.1 Scoring & multiplier
- Base points per enemy kill, scaled by enemy class and current multiplier (1×–10×, Geometry-Wars style), multiplier increases by collecting shards and decays over a few seconds without a pickup.
- Chain bonuses for killing an entire spawned "wave" without missing.

### 8.2 Lives & bombs
- 3 starting lives, extra life awarded at score thresholds.
- Bomb charge fills slowly over time / faster via shard combos; manual detonation via Z-trigger.

### 8.3 Seeds & replayability
- Title screen displays the current run's **cosmetic seed** (affects enemy silhouette variation and palette starting hue) and **difficulty seed** (affects spawn pattern RNG stream) separately, both enterable by hand so players can share/replay specific "looks" or challenge patterns — an homage to speedrun/challenge-seed culture, cheap to implement since generation is already fully parametric.

### 8.4 Accessibility / comfort options
- Given the deliberately psychedelic Space Giraffe-style background, a pause-menu **Background Intensity** slider (tunnel saturation, turbulence amplitude, screen-roll amount) and a **Reduce Flash** toggle (caps rapid brightness changes on bomb detonation) are first-class options, not an afterthought.

### 8.5 High score persistence
- Top scores + seeds stored via libdragon's EEPROM/Controller Pak save support.

---

## 9. Technical Architecture

### 9.1 Frame loop (target 60fps @ 320×240)
1. **Input poll** (controller state).
2. **Simulation step:** player movement, enemy AI/steering, projectile integration, collision (simple broad-phase grid + circle/segment checks suited to a bounded 2D-on-a-plane arena), spawn director tick, multiplier/score update.
3. **Procedural update pass:** advance tunnel ring recycling/noise phases, advance grid spring simulation, advance HSV palette rotation, tick audio sequencer/envelopes.
4. **Render — tiny3d pass:**
   - Bind/update tunnel ring buffer (partial update: only the recycled ring's data touched, not the whole tunnel).
   - Draw tunnel (opaque or lightly fogged, texture + vertex color, no lighting needed — self-illuminated look).
   - Draw grid plane (alpha blended, vertex-colored/pulsed).
   - Draw gameplay entities (ship, enemies, bullets) with simple flat/vertex-lit shading via tiny3d's lighting toggle, largely for the subtle self-illumination look vector-shooters use.
   - Draw particles/shards as camera-facing billboards, additive blend.
   - Draw UI (score, multiplier, lives) via `rdpq_text` / simple RDP quads.
5. **Audio mix step:** poll the shared libdragon mixer, which combines the procedural SFX voices with whatever `xm64player` music channels are currently playing into the next audio buffer chunk (kept decoupled from video framerate via libdragon's audio API double-buffering).
6. **Present / vsync.**

### 9.2 Frame-budget director
A lightweight profiler tracks RSP/RDP/CPU time per phase (libdragon exposes debug/profiling hooks for this). If the frame is trending over budget, the director first reduces tunnel ring count/turbulence complexity, then particle counts, before ever touching gameplay-critical elements (hit detection, input latency) — visual richness degrades gracefully under load, gameplay never does.

### 9.3 Memory plan (indicative, mandatory 8MB console w/ Expansion Pak)
| System | Approx. budget |
|---|---|
| Code + libdragon/tiny3d runtime | ~700KB–1MB |
| Procedural mesh buffers (tunnel rings, enemy species, ship, particles) — headroom raised since 8MB is a guaranteed floor, not a best case | ~600KB–1MB |
| Synthesized textures/gradients (small, generated once) | <150KB |
| Procedural SFX synth buffers + voice pool | ~150–250KB |
| XM64 music streaming buffers (samples pulled from ROM on demand, not fully resident) | ~150–300KB |
| Framebuffer(s) + Z-buffer — can afford triple-buffering or a higher internal resolution now that 8MB is guaranteed | ~450KB–900KB |
| Gameplay state (entities, particles, scoring, RNG streams) | ~250–400KB |
| Slack / stack / heap headroom | remainder (multiple MB free) |

Because the Expansion Pak is now a hard requirement rather than an opportunistic bonus, the 8MB figure above is the *only* budget the game needs to design against — there's no bare-4MB fallback path to maintain. This simplifies the mesh/particle systems and the frame-budget director (Section 9.2) alike, since they only ever need to scale within one known ceiling instead of degrading gracefully across two very different memory tiers.

### 9.4 Suggested module breakdown
```
/src
  main.c              -- boot, main loop, state machine
  input.c             -- controller polling, control-scheme mapping
  sim/
    player.c
    enemies.c          -- per-species behavior + generator hooks
    projectiles.c
    director.c         -- spawn pacing, difficulty/intensity curve
    collision.c
  gen/
    mesh_gen.c         -- parametric primitive + species generators
    palette_gen.c      -- HSV rotation, gradient texture synth
    tunnel_gen.c        -- ring math, recycling
    grid_sim.c          -- spring-damped grid displacement
  audio/
    synth.c             -- oscillators, envelopes, filters
    sfx.c                -- gameplay-triggered SFX parameter mapping
    music.c              -- .xm64 loading/playback via xm64player, intensity-tier track switching, effect-callback sync cues
  render/
    render_tunnel.c
    render_grid.c
    render_entities.c
    render_ui.c
  meta/
    scoring.c
    save.c               -- EEPROM high scores/seeds
    options.c            -- accessibility settings
```

---

## 10. Scope & Milestones

**M0 — Tech spike (2–3 weeks):** Boot libdragon+tiny3d project, render a single procedurally-generated rotating tunnel of rings with scrolling, confirm framerate headroom on Ares and/or Gopher64 (Homebrew Mode) and real hardware.

**M1 — Grid & ship (2 weeks):** Add transparent deformable grid plane over the tunnel, add player ship generator + analog-stick movement, confirm the "traveling through the tunnel" readability goal holds with the grid on top.

**M2 — Combat core (3–4 weeks):** C-button aim/fire, one enemy species (Wanderer), basic collision, procedural shot/explosion SFX, score/multiplier HUD.

**M3 — Bestiary & director (3 weeks):** Remaining enemy species, spawn director/difficulty curve, bomb system, palette/intensity linkage between combat state and tunnel/music.

**M4 — Audio pass (2 weeks):** Full procedural music sequencer, beat-synced grid pulsing, voice-priority tuning under heavy combat.

**M5 — Meta & polish (2–3 weeks):** Seeds UI, accessibility options, EEPROM high scores, frame-budget director, hardware compatibility pass across loader devices (64drive/EverDrive/SC64).

**M6 — Optimization & ship (ongoing):** Real-hardware profiling, RSP/RDP budget tightening, PAL/NTSC timing verification.

---

## 11. Key Risks & Mitigations

| Risk | Mitigation |
|---|---|
| RSP/RDP overdraw from two full-screen background layers (tunnel + alpha grid) plus foreground entities could blow the 60fps budget. | Keep tunnel/grid vertex counts modest and reuse fog/blend hardware units rather than per-pixel tricks; frame-budget director scales background complexity dynamically (Sec. 9.2). |
| Procedural SFX synthesis and XM64 music playback share the same mixer channel pool and CPU/RSP budget, and could compete during heavy swarms. | Reserve a fixed sub-pool of mixer channels for music (sized to the authored track's channel count), separate from a fixed SFX voice pool with priority-based stealing. XM64 playback is lightweight by design (~3% CPU / ~10% RSP for a 10-channel track per published libdragon figures), but this should still be profiled early (M4) rather than late. |
| tiny3d's "everything DMA'd each draw" model means per-object CPU-side bookkeeping (matrices, vertex buffers kept live in RDRAM) scales with entity count. | Cap concurrent on-screen enemies/particles; pool and reuse buffers rather than allocating per spawn. |
| N64 controller's lack of a second stick may make aiming feel imprecise compared to genre expectations. | Ship two first-class control schemes (Sec. 4) and playtest both; optional auto-aim-assist as an accessibility/comfort option, not a crutch that trivializes scoring (disable it from leaderboard-eligible runs if needed). |
| tiny3d/libdragon preview-branch API churn during development. | Pin a specific libdragon preview commit/tag for the project and upgrade deliberately at milestone boundaries, not continuously. |
| Requiring the Expansion Pak narrows the pool of original consoles that can run the ROM out of the box (vs. flashcart/emulator setups, where Pak presence is typically just a config toggle). | Treat this as a deliberate scope decision, not an oversight: detect Pak absence at boot and fail with a clear on-screen message rather than crashing or silently misbehaving; state the requirement prominently in any release notes or ROM listing. |
| .xm music re-introduces sampled instrument audio via streamed .xm64 playback — a deliberate, scoped exception to the project's otherwise-procedural philosophy, and one that adds ROM size the SFX/graphics systems don't. | Keep the exception explicit and contained to Section 7.3 rather than letting it creep elsewhere; lean on .xm64's ROM-streaming design (samples pulled on demand, not RAM-resident) to control the RAM cost, and budget ROM size for a small, curated set of authored tracks rather than an ever-growing adaptive library. |
| Psychedelic background could hurt readability or trigger discomfort. | Contrast pillar (Sec. 1.1 #3) enforced via lower background saturation/value than foreground at all times; Background Intensity + Reduce Flash options (Sec. 8.4) shipped from M5 onward, not bolted on late. |

---

## 12. Reference Inspirations
- **Geometry Wars: Retro Evolved** (Bizarre Creations) — core combat loop, multiplier/shard system, deformable neon grid floor.
- **Space Giraffe** (Llamasoft / Jeff Minter) — psychedelic tunnel/tube traversal aesthetic, audio-reactive background intensity, transparent playfield-over-tunnel presentation.
- **libdragon** (DragonMinded et al.) — open-source N64 SDK providing the OS/RDP/audio foundation this design targets.
- **tiny3d** (HailToDodongo) — from-scratch RSP 3D microcode/API used for all rendering described here.
- **libxm / XM64 player** (libdragon's tracker playback stack, adapted from Artefact2's libxm) — plays the hand-authored .xm soundtrack (Section 7.3), the one deliberately non-procedural system in the game. Tracks are composed in a conventional tracker such as MilkyTracker or OpenMPT.
