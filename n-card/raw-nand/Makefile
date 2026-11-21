.DEFAULT_GOAL := all
.SUFFIXES:
.DELETE_ON_ERROR:

R20_RULES := ../toolchain/r20.mk
include $(R20_RULES)

BUILD := build
CFLAGS := -march=armv4t -marm -mthumb-interwork -Os -std=gnu99 -Wall -Wextra \
          -ffreestanding -fomit-frame-pointer -fPIC -ffunction-sections -fdata-sections
CPPFLAGS += $(R20_INCLUDES)
DLDI_OBJECTS := $(addprefix $(BUILD)/,header.o interface.o nand.o)

.PHONY: all clean
all: $(BUILD)/nrio-raw-toolkit.dldi

$(BUILD)/%.o: source/%.c Makefile $(R20_RULES)
	@mkdir -p "$(@D)"
	"$(CC)" $(GCC_SEARCH) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" \
		-MT "$@" -S "$<" -o "$(@:.o=.s)"
	"$(AS)" "$(@:.o=.s)" -o "$@"

$(BUILD)/header.o: source/header.S Makefile $(R20_RULES)
	@mkdir -p "$(@D)"
	"$(AS)" "$<" -o "$@"

$(BUILD)/nrio-raw-toolkit.elf: $(DLDI_OBJECTS) linker.ld Makefile $(R20_RULES)
	"$(LD)" --gc-sections -T linker.ld -o "$@" $(DLDI_OBJECTS)

$(BUILD)/nrio-raw-toolkit.dldi: $(BUILD)/nrio-raw-toolkit.elf
	"$(OBJCOPY)" -O binary "$<" "$@"

clean:
	rm -rf "$(BUILD)"

-include $(DLDI_OBJECTS:.o=.d)
