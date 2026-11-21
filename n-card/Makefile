.DEFAULT_GOAL := all
.SUFFIXES:
.DELETE_ON_ERROR:

R20_RULES := toolchain/r20.mk
include $(R20_RULES)

ARCH := -march=armv4t -marm -mthumb-interwork -mtune=arm7tdmi
CPPFLAGS += $(R20_INCLUDES)
CFLAGS += $(ARCH) -std=gnu99 -O2 -Wall -ffreestanding -fomit-frame-pointer -fPIC
ASFLAGS += -mcpu=arm946e-s

SRC_DIRS := source/dldi source/card source/nand source/ftl source/system
C_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))
C_RECONSTRUCTION := source/card/nand_transfer.c source/nand/page_ops.c \
                    source/ftl/mapping.c source/ftl/write.c

# A plain build uses C. Verification defaults to the reference-matching variant.
C_ONLY ?= $(if $(filter verify,$(MAKECMDGOALS)),0,1)
ifeq ($(C_ONLY),1)
BUILD := build
ASM_SOURCES := source/dldi/header.S source/system/data.S
$(BUILD)/ftl/block_tables.o: CPPFLAGS += -DNRIO_C_ONLY
# Keep the page dispatcher's separate address loads for each switch case.
$(BUILD)/nand/page_ops.o: CFLAGS += -fno-gcse
else
BUILD := build/matching
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

# Retain the original block-table search loop entry.
$(BUILD)/ftl/block_tables.o: CFLAGS += -fno-tree-ch

$(C_OBJECTS): $(BUILD)/%.o: source/%.c Makefile $(R20_RULES) scripts/fix-got.sed
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" \
		-MT "$@" -S "$<" -o "$(@:.o=.gcc.s)"
	sed -E -f scripts/fix-got.sed "$(@:.o=.gcc.s)" > "$(@:.o=.s)"
	"$(AS)" $(ASFLAGS) "$(@:.o=.s)" -o "$@"

$(ASM_OBJECTS): $(BUILD)/%.o: source/%.S Makefile $(R20_RULES)
	@mkdir -p "$(@D)"
	"$(AS)" $(ASFLAGS) "$<" -o "$@"

$(BUILD)/nrio.elf: $(OBJECTS) linker/dldi.ld Makefile $(R20_RULES)
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
