CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude
AR      ?= ar

BUILD  := build
LIB    := $(BUILD)/libclib.a
SRCS   := $(wildcard src/*.c)
OBJS   := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
TESTS  := $(BUILD)/tests
BENCH  := $(BUILD)/bench

.PHONY: all test bench clean

all: $(LIB)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

test: $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/*.c $(LIB) -o $(TESTS)
	./$(TESTS)

bench: $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) bench/*.c $(LIB) -o $(BENCH)
	./$(BENCH)

clean:
	rm -rf $(BUILD)
