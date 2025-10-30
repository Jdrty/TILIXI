# dir
SRC_DIR := src
TESTS_DIR := tests
BUILD_DIR := build
INCLUDE := -I.

# compiler settings
CC := gcc
CFLAGS := -Wall -Wextra -std=c99
DEBUG := -g -O0

# find all .c files recursively under src/
SRC_FILES := $(shell find $(SRC_DIR) -name "*.c" -type f)

# filter out main.c for library build (tests don't need main)
SRC_LIB := $(filter-out $(SRC_DIR)/main.c, $(SRC_FILES))

# test files
TEST_FILES := $(shell find $(TESTS_DIR) -name "test_*.c" -type f)
TEST_NAMES := $(basename $(notdir $(TEST_FILES)))

# build targets
PRODUCTION := $(BUILD_DIR)/main
TEST_BINS := $(addprefix $(BUILD_DIR)/, $(TEST_NAMES))

# fefault target
all: $(PRODUCTION)

# vreate build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# build
$(PRODUCTION): $(BUILD_DIR) $(SRC_FILES)
	$(CC) $(CFLAGS) $(DEBUG) $(INCLUDE) $(SRC_FILES) -o $@
	@echo "(: built: $@"

# test
.PHONY: test
test: $(TEST_BINS)
	@echo "\nrunning tests...\n"
	@for test in $(TEST_BINS); do \
		echo "running: $$test"; \
		$$test || exit 1; \
		echo ""; \
	done
	@echo "(: all tests passed!"

# individual tests
$(BUILD_DIR)/test_%: $(BUILD_DIR) $(TESTS_DIR)/test_%.c $(SRC_LIB)
	$(CC) $(CFLAGS) $(DEBUG) $(INCLUDE) $< $(SRC_LIB) -o $@
	@echo "(: built: $@"

# utils
.PHONY: run
run: $(PRODUCTION)
	@./$(PRODUCTION)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ cleaned build directory"

.PHONY: rebuild
rebuild: clean all

.PHONY: help
help:
	@echo "...:"
	@echo "  make              - build production binary"
	@echo "  make run          - build and run production"
	@echo "  make test         - build and run all tests"
	@echo "  make rebuild      - clean and rebuild everything"
	@echo "  make clean        - remove build directory"
	@echo "  make help         - show this message"
