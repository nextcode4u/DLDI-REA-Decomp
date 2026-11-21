.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

DEVKITARM := $(DEVKITPRO)/devkitARM
GCC_LIBEXEC := $(DEVKITARM)/libexec/gcc/arm-eabi/4.1.1

ifeq ($(wildcard $(DEVKITARM)/bin/arm-eabi-gcc.exe),)
EXE :=
else
EXE := .exe
ifneq ($(shell command -v wslpath),)
tool_path = $(shell wslpath -m "$(1)")
endif
endif
tool_path ?= $(1)

CC := $(DEVKITARM)/bin/arm-eabi-gcc$(EXE)
AS := $(DEVKITARM)/bin/arm-eabi-as$(EXE)
LD := $(DEVKITARM)/bin/arm-eabi-ld$(EXE)
OBJCOPY := $(DEVKITARM)/bin/arm-eabi-objcopy$(EXE)

BUILD := build
CFLAGS := -march=armv4t -marm -mthumb-interwork -Os -std=gnu99 -Wall -Wextra \
          -ffreestanding -fomit-frame-pointer -fPIC -ffunction-sections -fdata-sections
CPPFLAGS := -DARM9 -isystem "$(call tool_path,$(DEVKITPRO)/libnds/include)" \
            -isystem "$(call tool_path,$(DEVKITARM)/arm-eabi/include)"
GCC_SEARCH := -B"$(call tool_path,$(GCC_LIBEXEC))/" -B"$(call tool_path,$(DEVKITARM)/bin)/"

.PHONY: all clean
all: $(BUILD)/nrio-raw-toolkit.dldi

$(BUILD)/toolkit/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/toolkit/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" "$<" -o "$@"

DLDI_OBJECTS := $(addprefix $(BUILD)/toolkit/,header.o interface.o nand.o)

$(BUILD)/nrio-raw-toolkit.elf: $(DLDI_OBJECTS) linker.ld
	"$(LD)" --gc-sections -T linker.ld -o "$@" $(DLDI_OBJECTS)

$(BUILD)/%.dldi: $(BUILD)/%.elf
	"$(OBJCOPY)" -O binary "$<" "$@"

clean:
	rm -rf "$(BUILD)"

-include $(DLDI_OBJECTS:.o=.d)
