.DEFAULT_GOAL := all
.SUFFIXES:
.DELETE_ON_ERROR:

# The Windows r20 archive also runs from WSL, with explicit search paths.
DEVKITARM := $(DEVKITPRO)/devkitARM
GCC_VERSION := 4.1.1
GCC_LIBDIR := $(DEVKITARM)/lib/gcc/arm-eabi/$(GCC_VERSION)
GCC_LIBEXEC := $(DEVKITARM)/libexec/gcc/arm-eabi/$(GCC_VERSION)

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(wildcard $(GCC_LIBDIR)/crtbegin.o),)
$(error Set DEVKITPRO to the SDK root containing devkitARM r20 and libnds)
endif
include $(DEVKITARM)/ds_rules
endif

ifneq ($(wildcard $(DEVKITARM)/bin/arm-eabi-gcc.exe),)
CC := $(DEVKITARM)/bin/arm-eabi-gcc.exe
AS := $(DEVKITARM)/bin/arm-eabi-as.exe
LD := $(DEVKITARM)/bin/arm-eabi-ld.exe
OBJCOPY := $(DEVKITARM)/bin/arm-eabi-objcopy.exe
ifneq ($(shell command -v wslpath),)
tool_path = $(shell wslpath -m "$(1)")
endif
else
LD := $(DEVKITARM)/bin/arm-eabi-ld
endif
tool_path ?= $(1)

GCC_SEARCH := -B"$(call tool_path,$(GCC_LIBEXEC))/" \
              -B"$(call tool_path,$(DEVKITARM)/bin)/" \
              -isystem "$(call tool_path,$(DEVKITARM)/arm-eabi/include)"
ARCH := -march=armv4t -marm -mthumb-interwork -mtune=arm7tdmi
CPPFLAGS += -DARM9 -isystem "$(call tool_path,$(LIBNDS)/include)"
CFLAGS += $(ARCH) -std=gnu99 -O2 -Wall -ffreestanding -fomit-frame-pointer -fPIC
ASFLAGS += -mcpu=arm946e-s

SRC_DIRS := source/dldi source/card source/nand source/ftl source/system
C_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))
C_RECONSTRUCTION := source/card/nand_transfer.c source/nand/page_ops.c \
                    source/ftl/block_tables.c source/ftl/mapping.c source/ftl/write.c

ifeq ($(C_ONLY),1)
BUILD := build/c-only
ASM_SOURCES := source/dldi/header.S source/system/data.S
else
BUILD := build
C_SOURCES := $(filter-out $(C_RECONSTRUCTION),$(C_SOURCES))
endif
C_OBJECTS := $(C_SOURCES:source/%.c=$(BUILD)/%.o)
ASM_OBJECTS := $(ASM_SOURCES:source/%.S=$(BUILD)/%.o)
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)
START_FILES := $(call tool_path,$(GCC_LIBDIR)/crti.o) $(call tool_path,$(GCC_LIBDIR)/crtbegin.o)
END_FILES := $(call tool_path,$(GCC_LIBDIR)/crtend.o) $(call tool_path,$(GCC_LIBDIR)/crtn.o)

.PHONY: all clean verify
all: $(BUILD)/nrio.dldi

# These routines call only ARM code. GCC can then emit a direct tail branch.
$(BUILD)/ftl/sector_read.o: CFLAGS += -mno-thumb-interwork

$(C_OBJECTS): $(BUILD)/%.o: source/%.c Makefile scripts/fix-got.sed
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" \
		-MT "$@" -S "$<" -o "$(@:.o=.gcc.s)"
	sed -E -f scripts/fix-got.sed "$(@:.o=.gcc.s)" > "$(@:.o=.s)"
	"$(AS)" $(ASFLAGS) "$(@:.o=.s)" -o "$@"

$(ASM_OBJECTS): $(BUILD)/%.o: source/%.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" $(ASFLAGS) "$<" -o "$@"

$(BUILD)/nrio.elf: $(OBJECTS) linker/dldi.ld Makefile
	"$(LD)" -T linker/dldi.ld -Map "$(BUILD)/nrio.map" -o "$@" \
		$(foreach file,$(START_FILES),"$(file)") $(OBJECTS) \
		-L"$(call tool_path,$(GCC_LIBDIR))" -lgcc $(foreach file,$(END_FILES),"$(file)")

$(BUILD)/nrio.dldi: $(BUILD)/nrio.elf
	"$(OBJCOPY)" -O binary "$<" "$@"

REFERENCE ?= reference/nrio.dldi
verify: $(BUILD)/nrio.dldi
	cmp "$(REFERENCE)" "$<"
	sha256sum "$<"

clean:
	rm -r -f "$(BUILD)"

-include $(C_SOURCES:source/%.c=$(BUILD)/%.d)
