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
          -ffreestanding -fomit-frame-pointer -fPIC
CPPFLAGS := -DARM9 -isystem "$(call tool_path,$(DEVKITPRO)/libnds/include)" \
            -isystem "$(call tool_path,$(DEVKITARM)/arm-eabi/include)"
GCC_SEARCH := -B"$(call tool_path,$(GCC_LIBEXEC))/" -B"$(call tool_path,$(DEVKITARM)/bin)/"

.PHONY: all legacy clean
all: $(BUILD)/nrio-raw-toolkit.dldi
legacy: $(BUILD)/nrio-raw-readonly.dldi $(BUILD)/nrio-raw-recovery.dldi $(BUILD)/nrio-raw-spare-readonly.dldi $(BUILD)/nrio-raw-spare-write.dldi $(BUILD)/nrio-raw-protected.dldi $(BUILD)/nrio-raw-banner.dldi

$(BUILD)/toolkit/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -DRAW_NAND_WRITE -DRAW_NAND_TOOLKIT -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/toolkit/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" --defsym RAW_NAND_TOOLKIT=1 "$<" -o "$@"

TOOLKIT_OBJECTS := $(addprefix $(BUILD)/toolkit/,header.o interface.o nand.o restore.o)

$(BUILD)/nrio-raw-toolkit.elf: $(TOOLKIT_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(TOOLKIT_OBJECTS)

$(BUILD)/read-only/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/recovery/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -DRAW_NAND_WRITE -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/spare/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -DRAW_NAND_SPARE -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/spare-write/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -DRAW_NAND_SPARE -DRAW_NAND_WRITE -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/protected/interface.o: source/interface.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -DRAW_NAND_WRITE -DRAW_NAND_PROTECT_BOOT -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/protected/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" --defsym RAW_NAND_WRITE=1 --defsym RAW_NAND_PROTECT_BOOT=1 "$<" -o "$@"

$(BUILD)/banner/%.o: source/%.c Makefile
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -DRAW_NAND_WRITE -DRAW_NAND_BANNER -MMD -MP -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/banner/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" --defsym RAW_NAND_WRITE=1 --defsym RAW_NAND_BANNER=1 "$<" -o "$@"

$(BUILD)/read-only/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" "$<" -o "$@"

$(BUILD)/recovery/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" --defsym RAW_NAND_WRITE=1 "$<" -o "$@"

$(BUILD)/spare/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" --defsym RAW_NAND_SPARE=1 "$<" -o "$@"

$(BUILD)/spare-write/header.o: source/header.S Makefile
	@mkdir -p "$(@D)"
	"$(AS)" --defsym RAW_NAND_SPARE=1 --defsym RAW_NAND_WRITE=1 "$<" -o "$@"

READ_ONLY_OBJECTS := $(addprefix $(BUILD)/read-only/,header.o interface.o nand.o)
SPARE_OBJECTS := $(addprefix $(BUILD)/spare/,header.o interface.o nand.o)
SPARE_WRITE_OBJECTS := $(addprefix $(BUILD)/spare-write/,header.o interface.o nand.o restore.o)
RECOVERY_OBJECTS := $(addprefix $(BUILD)/recovery/,header.o interface.o nand.o restore.o)

PROTECTED_OBJECTS := $(addprefix $(BUILD)/protected/,header.o interface.o) $(addprefix $(BUILD)/recovery/,nand.o restore.o)

BANNER_OBJECTS := $(addprefix $(BUILD)/banner/,header.o interface.o restore.o) $(BUILD)/recovery/nand.o

$(BUILD)/nrio-raw-banner.elf: $(BANNER_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(BANNER_OBJECTS)

$(BUILD)/nrio-raw-protected.elf: $(PROTECTED_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(PROTECTED_OBJECTS)

$(BUILD)/nrio-raw-readonly.elf: $(READ_ONLY_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(READ_ONLY_OBJECTS)

$(BUILD)/nrio-raw-recovery.elf: $(RECOVERY_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(RECOVERY_OBJECTS)

$(BUILD)/nrio-raw-spare-readonly.elf: $(SPARE_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(SPARE_OBJECTS)

$(BUILD)/nrio-raw-spare-write.elf: $(SPARE_WRITE_OBJECTS) linker.ld
	"$(LD)" -T linker.ld -o "$@" $(SPARE_WRITE_OBJECTS)

$(BUILD)/%.dldi: $(BUILD)/%.elf
	"$(OBJCOPY)" -O binary "$<" "$@"

clean:
	rm -rf "$(BUILD)"

-include $(TOOLKIT_OBJECTS:.o=.d) $(READ_ONLY_OBJECTS:.o=.d) $(RECOVERY_OBJECTS:.o=.d) $(SPARE_OBJECTS:.o=.d) $(SPARE_WRITE_OBJECTS:.o=.d) $(PROTECTED_OBJECTS:.o=.d) $(BANNER_OBJECTS:.o=.d)
