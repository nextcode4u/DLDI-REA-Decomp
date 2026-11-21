DEVKITARM := $(DEVKITPRO)/devkitARM
GCC_LIBDIR := $(DEVKITARM)/lib/gcc/arm-eabi/4.1.1
GCC_LIBEXEC := $(DEVKITARM)/libexec/gcc/arm-eabi/4.1.1
TOOL_PREFIX := $(DEVKITARM)/bin/arm-eabi-

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(wildcard $(GCC_LIBDIR)/crtbegin.o),)
$(error Set DEVKITPRO to the SDK root containing devkitARM r20 and libnds)
endif
include $(DEVKITARM)/ds_rules

# Native shells, including MSYS2, use the usual extensionless tool names.
tool_path = $(1)
tool_suffix :=
# WSL needs both the Windows extension and translated GCC search paths.
ifeq ($(wildcard $(TOOL_PREFIX)gcc),)
ifneq ($(and $(wildcard $(TOOL_PREFIX)gcc.exe),$(shell command -v wslpath)),)
tool_suffix := .exe
tool_path = $(shell wslpath -m "$(1)")
endif
endif

CC := $(TOOL_PREFIX)gcc$(tool_suffix)
AS := $(TOOL_PREFIX)as$(tool_suffix)
LD := $(TOOL_PREFIX)ld$(tool_suffix)
OBJCOPY := $(TOOL_PREFIX)objcopy$(tool_suffix)

GCC_SEARCH := -B"$(call tool_path,$(GCC_LIBEXEC))/" -B"$(call tool_path,$(DEVKITARM)/bin)/"
R20_INCLUDES := -DARM9 -isystem "$(call tool_path,$(LIBNDS)/include)" \
                -isystem "$(call tool_path,$(DEVKITARM)/arm-eabi/include)"
endif
