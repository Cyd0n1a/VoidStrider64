BUILD_DIR = build

include $(N64_INST)/include/n64.mk
include tiny3d/t3d.mk

N64_ROM_TITLE = "VOIDSTRIDER64"

all: voidstrider64.z64

OBJS = \
    $(BUILD_DIR)/src/main.o \
    $(BUILD_DIR)/src/input/input.o \
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
    $(BUILD_DIR)/src/audio/synth.o \
    $(BUILD_DIR)/src/render/render.o \
    $(BUILD_DIR)/src/render/render_entities.o \
    $(BUILD_DIR)/src/render/render_ui.o

voidstrider64.z64: $(BUILD_DIR)/voidstrider64.elf
$(BUILD_DIR)/voidstrider64.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) voidstrider64.z64 voidstrider64.elf.sym

-include $(OBJS:.o=.d)

.PHONY: all clean
