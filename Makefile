BUILD_DIR = build

include $(N64_INST)/include/n64.mk
include tiny3d/t3d.mk

N64_ROM_TITLE = "VOIDSTRIDER64"
N64_ROM_SAVETYPE = eeprom4k

all: voidstrider64.z64

# --- ROM filesystem assets (GDD 7.3: music is the one authored exception) ---
filesystem/title.xm64: assets/music/title.xm
	@mkdir -p filesystem
	$(N64_AUDIOCONV) -o filesystem $<

filesystem/gameplay.wav64: assets/music/gameplay.wav
	@mkdir -p filesystem
	$(N64_AUDIOCONV) --wav-compress 1 -o filesystem $<

# Boot splash logos + sounds (authored, sanctioned exception like the
# music). PNGs/jingle-WAV under assets/splash/gen/ are produced by the
# host ffmpeg step in build.sh.
filesystem/ld_logo.sprite: assets/splash/gen/ld_logo.png
	@mkdir -p filesystem
	$(N64_MKSPRITE) -f RGBA16 -o filesystem $<

filesystem/cydonis.sprite: assets/splash/gen/cydonis.png
	@mkdir -p filesystem
	$(N64_MKSPRITE) -f RGBA16 -o filesystem $<

filesystem/dragon.wav64: assets/splash/dragon.wav
	@mkdir -p filesystem
	$(N64_AUDIOCONV) --wav-compress 1 -o filesystem $<

filesystem/jingle.wav64: assets/splash/gen/jingle.wav
	@mkdir -p filesystem
	$(N64_AUDIOCONV) --wav-compress 1 -o filesystem $<

$(BUILD_DIR)/voidstrider64.dfs: filesystem/title.xm64 filesystem/gameplay.wav64 \
    filesystem/ld_logo.sprite filesystem/cydonis.sprite \
    filesystem/dragon.wav64 filesystem/jingle.wav64

OBJS = \
    $(BUILD_DIR)/src/main.o \
    $(BUILD_DIR)/src/input/input.o \
    $(BUILD_DIR)/src/input/rumble.o \
    $(BUILD_DIR)/src/sim/player.o \
    $(BUILD_DIR)/src/sim/enemies.o \
    $(BUILD_DIR)/src/sim/projectiles.o \
    $(BUILD_DIR)/src/sim/shards.o \
    $(BUILD_DIR)/src/sim/director.o \
    $(BUILD_DIR)/src/sim/bomb.o \
    $(BUILD_DIR)/src/gen/palette_gen.o \
    $(BUILD_DIR)/src/gen/tunnel_gen.o \
    $(BUILD_DIR)/src/gen/grid_sim.o \
    $(BUILD_DIR)/src/gen/mesh_gen.o \
    $(BUILD_DIR)/src/meta/scoring.o \
    $(BUILD_DIR)/src/meta/options.o \
    $(BUILD_DIR)/src/meta/save.o \
    $(BUILD_DIR)/src/meta/fortunes.o \
    $(BUILD_DIR)/src/audio/synth.o \
    $(BUILD_DIR)/src/audio/music.o \
    $(BUILD_DIR)/src/render/render.o \
    $(BUILD_DIR)/src/render/splash.o \
    $(BUILD_DIR)/src/render/render_entities.o \
    $(BUILD_DIR)/src/render/render_ui.o

voidstrider64.z64: $(BUILD_DIR)/voidstrider64.elf $(BUILD_DIR)/voidstrider64.dfs
$(BUILD_DIR)/voidstrider64.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) filesystem voidstrider64.z64 voidstrider64.elf.sym

-include $(OBJS:.o=.d)

.PHONY: all clean
