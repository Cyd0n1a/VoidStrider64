BUILD_DIR = build

include $(N64_INST)/include/n64.mk
include tiny3d/t3d.mk

N64_ROM_TITLE = "VOIDSTRIDER64"

all: voidstrider64.z64

OBJS = \
    $(BUILD_DIR)/src/main.o \
    $(BUILD_DIR)/src/input/input.o \
    $(BUILD_DIR)/src/gen/palette_gen.o \
    $(BUILD_DIR)/src/gen/tunnel_gen.o \
    $(BUILD_DIR)/src/render/render.o

voidstrider64.z64: $(BUILD_DIR)/voidstrider64.elf
$(BUILD_DIR)/voidstrider64.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) voidstrider64.z64 voidstrider64.elf.sym

.PHONY: all clean
