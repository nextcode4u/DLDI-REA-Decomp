CC ?= cc
AR ?= ar
BUILD ?= build
CFLAGS ?= -O2 -g

OBJECTS := $(addprefix $(BUILD)/,updater.o payload.o runtime.o format-native.o)

.PHONY: all
all: $(BUILD)/libudisk.a

$(BUILD)/libudisk.a: $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $@

-include $(OBJECTS:.o=.d)
