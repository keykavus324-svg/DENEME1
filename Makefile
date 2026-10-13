CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude
AR      ?= ar

BUILD  := build
EXDIR  := examples
LIB    := $(BUILD)/libclib.a
SRCS   := $(wildcard src/*.c)
OBJS   := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
TESTS  := $(BUILD)/tests
BENCH  := $(BUILD)/bench
EXES   := $(patsubst $(EXDIR)/%.c,$(BUILD)/%,$(wildcard $(EXDIR)/*.c))

.PHONY: all test bench examples clean

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

examples: $(LIB) $(EXES)

$(BUILD)/%: $(EXDIR)/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) -o $@

clean:
	rm -rf $(BUILD)
