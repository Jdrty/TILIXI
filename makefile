# directories
SRC_DIR := src
TESTS_DIR := tests
BUILD_DIR := build
INCLUDE := -Iinclude $(shell find src -type d -exec printf -- "-I%s " {} \;)

# compiler settings
CC := gcc
CFLAGS := -Wall -Wextra -std=c99

# debug
ifeq ($(DEBUG),1)
    DEBUG_FLAGS := -g -O0 -DDEBUG
else
    DEBUG_FLAGS := -O2
endif

CFLAGS := -Wall -Wextra -std=c99 $(DEBUG_FLAGS)

# find all .c files recursively under src/
# exclude ESP32-specific files for PC builds
SRC_FILES := $(shell find $(SRC_DIR) -name "*.c" -type f | grep -v platform/esp32 | grep -v filesystem)
# include VFS stub for tests
SRC_FILES += $(SRC_DIR)/filesystem/vfs/vfs_stub.c

# filter out platform-specific main files for library build (tests don't need main)
SRC_LIB := $(filter-out $(SRC_DIR)/platform/pc/main.c, $(SRC_FILES))

# test files
TEST_FILES := $(shell find $(TESTS_DIR) -name "test_*.c" -type f)
TEST_NAMES := $(basename $(notdir $(TEST_FILES)))

# build targets
PRODUCTION := $(BUILD_DIR)/main
TEST_BINS := $(addprefix $(BUILD_DIR)/, $(TEST_NAMES))

# default target
all: $(PRODUCTION)

# create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# build
$(PRODUCTION): $(BUILD_DIR) $(SRC_FILES)
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC_FILES) -o $@
	@echo "(: built: $@"

# test
.PHONY: test
test: $(TEST_BINS)
	@echo "running tests...\n"
	@for test in $(TEST_BINS); do \
		echo "running: $$test"; \
		$$test || exit 1; \
		echo ""; \
	done
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡤⠶⠚⠉⢉⣩⠽⠟⠛⠛⠛⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⠞⠉⠀⢀⣠⠞⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡞⠁⠀⠀⣰⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⠀⠀⠀⡼⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣠⡤⠤⠄⢤⣄⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⠀⠀⢰⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠴⠒⠋⠉⠀⠀⠀⣀⣤⠴⠒⠋⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⡄⠀⠀⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⠞⢳⡄⢀⡴⠚⠉⠀⠀⠀⠀⠀⣠⠴⠚⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⡀⠘⣧⠀⠀⠀⠀⠀⠀⠀⠀⣰⠃⠀⠀⠹⡏⠀⠀⠀⠀⠀⣀⣴⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠳⢬⣳⣄⣠⠤⠤⠶⠶⠒⠋⠀⠀⠀⠀⠹⡀⠀⠀⠀⠀⠈⠉⠛⠲⢦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠤⠖⠋⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠱⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⢳⠦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⣠⠖⠋⠀⠀⠀⣠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢱⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⠀⢃⠈⠙⠲⣄⡀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⢠⠞⠁⠀⠀⠀⢀⢾⠃⠀⠀⠀⠀⠀⠀⠀⠀⢢⠀⠀⠀⠀⠀⠀⠀⢣⠀⠀⠀⠀⠀⠀⠀⠀⠀⣹⠮⣄⠀⠀⠀⠙⢦⡀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⣰⠋⠀⠀⢀⡤⡴⠃⠈⠦⣀⠀⠀⠀⠀⠀⠀⢀⣷⢸⠀⠀⠀⠀⢀⣀⠘⡄⠤⠤⢤⠔⠒⠂⠉⠁⠀⠀⠀⠑⢄⡀⠀⠀⠙⢦⡀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⣼⠃⠀⠀⢠⣞⠟⠀⠀⠀⡄⠀⠉⠒⠢⣤⣤⠄⣼⢻⠸⠀⠀⠀⠀⠉⢤⠀⢿⡖⠒⠊⢦⠤⠤⣀⣀⡀⠀⠀⠀⠈⠻⡝⠲⢤⣀⠙⢦⠀⠀"
	@echo "⠀⠀⠀⢰⠃⠀⠀⣴⣿⠎⠀⠀⢀⣜⠤⠄⢲⠎⠉⠀⠀⡼⠸⠘⡄⡇⠀⠀⠀⠀⢸⠀⢸⠘⢆⠀⠘⡄⠀⠀⠀⢢⠉⠉⠀⠒⠒⠽⡄⠀⠈⠙⠮⣷⡀"
	@echo "⠀⠀⠀⡟⠀⠀⣼⢻⠧⠐⠂⠉⡜⠀⠀⡰⡟⠀⠀⠀⡰⠁⡇⠀⡇⡇⠀⠀⠀⠀⢺⠇⠀⣆⡨⢆⠀⢽⠀⠀⠀⠈⡷⡄⠀⠀⠀⠀⠹⡄⠀⠀⠀⠈⠁"
	@echo "⠀⠀⢸⠃⠀⠀⢃⠎⠀⠀⠀⣴⠃⠀⡜⠹⠁⠀⠀⡰⠁⢠⠁⠀⢸⢸⠀⠀⠀⢠⡸⢣⠔⡏⠀⠈⢆⠀⣇⠀⠀⠀⢸⠘⢆⠀⠀⠀⠀⢳⠀⠀⠀⠀⠀"
	@echo "⠀⠀⢸⠀⠀⠀⡜⠀⠀⢀⡜⡞⠀⡜⠈⠏⠀⠈⡹⠑⠒⠼⡀⠀⠀⢿⠀⠀⠀⢀⡇⠀⢇⢁⠀⠀⠈⢆⢰⠀⠀⠀⠈⡄⠈⢢⠀⠀⠀⠈⣇⠀⠀⠀⠀"
	@echo "⠀⠀⢸⡀⠀⢰⠁⠀⢀⢮⠀⠇⡜⠀⠘⠀⠀⢰⠃⠀⠀⡇⠈⠁⠀⢘⡄⠀⠀⢸⠀⠀⣘⣼⠤⠤⠤⣈⡞⡀⠀⠀⠀⡇⠰⡄⢣⡀⠀⠀⢻⠀⠀⠀⠀"
	@echo "⠀⠀⠈⡇⠀⡜⠀⢀⠎⢸⢸⢰⠁⠀⠄⠀⢠⠃⠀⠀⢸⠀⠀⠀⠀⠀⡇⠀⠀⡆⠀⠀⣶⣿⡿⠿⡛⢻⡟⡇⠀⠀⠀⡇⠀⣿⣆⢡⠀⠀⢸⡇⠀⠀⠀"
	@echo "⠀⠀⢠⡏⠀⠉⢢⡎⠀⡇⣿⠊⠀⠀⠀⢠⡏⠀⠀⠀⠎⠀⠀⠀⠀⠀⡇⠀⡸⠀⠀⠀⡇⠀⢰⡆⡇⢸⢠⢹⠀⠀⠀⡇⠀⢹⠈⢧⣣⠀⠘⡇⠀⠀⠀"
	@echo "⠀⠀⢸⡇⠀⠀⠀⡇⠀⡇⢹⠀⠀⠀⢀⡾⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⢠⠃⠀⠀⠠⠟⡯⣻⣇⢃⠇⢠⠏⡇⠀⢸⡆⠀⢸⠀⠈⢳⡀⠀⡇⠀⠀⠀"
	@echo "⠀⠀⠀⣇⠀⡔⠋⡇⠀⢱⢼⠀⠀⡂⣼⡇⢹⣶⣶⣶⣤⣤⣀⠀⠀⠀⣇⠇⠀⠀⠀⠀⣶⡭⢃⣏⡘⠀⡎⠀⠇⠀⡾⣷⠀⣼⠀⠀⠀⢻⡄⡇⠀⠀⠀"
	@echo "⠀⠀⠀⣹⠜⠋⠉⠓⢄⡏⢸⠀⠀⢳⡏⢸⠹⢀⣉⢭⣻⡽⠿⠛⠓⠀⠋⠀⠀⠀⠀⠀⠘⠛⠛⠓⠀⡄⡇⠀⢸⢰⡇⢸⡄⡟⠀⠀⠀⠀⢳⡇⠀⠀⠀"
	@echo "⠀⣠⠞⠁⠀⠀⠀⠀⠀⢙⠌⡇⠀⣿⠁⠀⡇⡗⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠀⠀⠀⠀⠀⠀⠁⠁⠀⢸⣼⠀⠈⣇⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⢸⠁⠀⠀⢀⡠⠔⠚⠉⠉⢱⣇⢸⢧⠀⠀⠸⣱⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⡤⠦⡔⠀⠀⠀⠀⠀⢀⡼⠀⠀⣼⡏⠀⠀⢹⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⢸⠀⠀⠀⠋⠀⠀⠀⢀⡠⠤⣿⣾⣇⣧⠀⠀⢫⡆⠀⠀⠀⠀⠀⠀⠀⢨⠀⠀⣠⠇⠀⠀⢀⡠⣶⠋⠀⠀⡸⣾⠁⠀⠀⠈⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⢸⡄⠀⠀⠀⠀⠠⠊⠁⠀⠀⢸⢃⠘⡜⡵⡀⠈⢿⡱⢲⡤⠤⢀⣀⣀⡀⠉⠉⣀⡠⡴⠚⠉⣸⢸⠀⠀⢠⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⢧⠀⠀⠀⠀⠀⠀⠀⣀⠤⠚⠚⣤⣵⡰⡑⡄⠀⢣⡈⠳⡀⠀⠀⠀⢨⡋⠙⣆⢸⠀⠀⣰⢻⡎⠀⠀⡎⡇⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠈⢷⡀⠀⠀⠀⠀⠀⠁⠀⠀⠀⡸⢌⣳⣵⡈⢦⡀⠳⡀⠈⢦⡀⠀⠘⠏⠲⣌⠙⢒⠴⡧⣸⡇⠀⡸⢸⠇⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⢠⣿⠢⡀⠀⠀⠀⠠⠄⡖⠋⠀⠀⠙⢿⣳⡀⠑⢄⠹⣄⡀⠙⢄⡠⠤⠒⠚⡖⡇⠀⠘⣽⡇⢠⠃⢸⢀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⣾⠃⠀⠀⠀⠀⠀⢀⡼⣄⠀⠀⠀⠀⠀⠑⣽⣆⠀⠑⢝⡍⠒⠬⢧⣀⡠⠊⠀⠸⡀⠀⢹⡇⡎⠀⡿⢸⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⡼⠁⠀⠀⠀⠀⠀⠀⢀⠻⣺⣧⠀⠀⠀⠰⢢⠈⢪⡷⡀⠀⠙⡄⠀⠀⠱⡄⠀⠀⠀⢧⠀⢸⡻⠀⢠⡇⣾⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⢰⠇⠀⠀⠀⠀⠀⠀⠀⢸⠀⡏⣿⠀⠀⠀⠀⢣⢇⠀⠑⣄⠀⠀⠸⡄⠀⠀⠘⡄⠀⠀⠸⡀⢸⠁⠀⡾⢰⡏⢳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "all tests passed!"

# individual test target - FIX: use $(TESTS_DIR)/test_$*.c instead of $<
$(BUILD_DIR)/test_%: $(BUILD_DIR) $(TESTS_DIR)/test_%.c $(SRC_LIB)
	$(CC) $(CFLAGS) $(DEBUG) $(INCLUDE) $(TESTS_DIR)/test_$*.c $(SRC_LIB) -o $@
	@echo "(: built: $@"

# utils
.PHONY: run
run: $(PRODUCTION)
	@./$(PRODUCTION)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "cleaned build directory"

.PHONY: rebuild
rebuild: clean all

# ESP32/PlatformIO targets
.PHONY: esp32-build esp32-upload esp32-monitor esp32-clean esp32-all esp32-small

# check if PlatformIO is available
PIO := $(shell command -v pio 2> /dev/null)
ifndef PIO
    PIO := $(shell command -v platformio 2> /dev/null)
endif

esp32-build:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building for ESP32-S3..."
	$(PIO) run -e esp32-s3-devkitc-1
	@echo "(: ESP32 build complete"

esp32-upload:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found....."; \
		exit 1; \
	fi
	@echo "uploading to ESP32-S3..."
	$(PIO) run -e esp32-s3-devkitc-1 -t upload
	@echo "(: upload complete"

esp32-monitor:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found..............."; \
		exit 1; \
	fi
	@echo "opening serial monitor (ctrl+c to exit)..."
	$(PIO) device monitor -e esp32-s3-devkitc-1

esp32-clean:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found."; \
		exit 1; \
	fi
	$(PIO) run -e esp32-s3-devkitc-1 -t clean
	@echo "(: ESP32 build cleaned"

esp32-all: esp32-build esp32-upload
	@echo "(: ESP32 build and upload complete"

esp32-small: esp32-all
	@echo "(: waiting 2 seconds for device to reset..."
	@sleep 2
	@echo "(: opening serial monitor (ctrl+c to exit)..."
	@echo "(: NOTE: You can now type in the serial monitor to interact with the TTY interface."
	$(PIO) device monitor -e esp32-s3-devkitc-1

# Filesystem initialization utility targets
.PHONY: filesystem-build filesystem-upload

filesystem-build:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building filesystem initialization utility for ESP32-S3..."
	$(PIO) run -e esp32-s3-filesystem
	@echo "(: filesystem initialization utility build complete"

filesystem-upload:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found....."; \
		exit 1; \
	fi
	@echo "uploading all filesystem upload utilities..."
	@for file in src/filesystem/upload/*.cpp; do \
		if [ -f "$$file" ]; then \
			basename=$$(basename "$$file" .cpp); \
			env_name="esp32-s3-$$basename"; \
			echo "uploading $$basename..."; \
			$(PIO) run -e $$env_name -t upload || exit 1; \
		fi; \
	done
	@echo "(: all filesystem upload utilities uploaded"

# file read test target
.PHONY: file-read

file-read:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building file read test for ESP32-S3..."
	$(PIO) run -e esp32-s3-file-read
	@echo "uploading file read test to ESP32-S3..."
	$(PIO) run -e esp32-s3-file-read -t upload
	@echo "(: file read test uploaded"
	@echo "(: waiting 2 seconds for device to reset..."
	@sleep 2
	@echo "(: opening serial monitor (ctrl+c to exit)..."
	@echo "(: NOTE: You should see output immediately. If not, try resetting the device."
	$(PIO) device monitor -e esp32-s3-file-read --baud 115200

# SD card read/write/delete test target
.PHONY: sdtest

sdtest:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building SD card test for ESP32-S3..."
	$(PIO) run -e esp32-s3-sdtest
	@echo "uploading SD card test to ESP32-S3..."
	$(PIO) run -e esp32-s3-sdtest -t upload
	@echo "(: SD card test uploaded"
	@echo "(: waiting 2 seconds for device to reset..."
	@sleep 2
	@echo "(: opening serial monitor (ctrl+c to exit)..."
	@echo "(: NOTE: You should see output immediately. If not, try resetting the device."
	$(PIO) device monitor -e esp32-s3-sdtest --baud 115200

# SD card byte-level test target
.PHONY: esp32-sdbyte

esp32-sdbyte:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building SD card byte test for ESP32-S3..."
	$(PIO) run -e esp32-s3-sdbyte
	@echo "uploading SD card byte test to ESP32-S3..."
	$(PIO) run -e esp32-s3-sdbyte -t upload
	@echo "(: SD card byte test uploaded"
	@echo "(: waiting 2 seconds for device to reset..."
	@sleep 2
	@echo "(: opening serial monitor (ctrl+c to exit)..."
	@echo "(: NOTE: You should see output immediately. If not, try resetting the device."
	$(PIO) device monitor -e esp32-s3-sdbyte --baud 115200

# SD card block-level test target
.PHONY: esp32-sdblock

esp32-sdblock:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building SD card block test for ESP32-S3..."
	$(PIO) run -e esp32-s3-sdblock
	@echo "uploading SD card block test to ESP32-S3..."
	$(PIO) run -e esp32-s3-sdblock -t upload
	@echo "(: SD card block test uploaded"
	@echo "(: waiting 2 seconds for device to reset..."
	@sleep 2
	@echo "(: opening serial monitor (ctrl+c to exit)..."
	@echo "(: NOTE: You should see output immediately. If not, try resetting the device."
	$(PIO) device monitor -e esp32-s3-sdblock --baud 115200

# serial monitor echo test target
.PHONY: esp32-serial-echo

esp32-serial-echo:
	@if [ -z "$(PIO)" ]; then \
		echo "error: PlatformIO not found. install it now!! >:[ "; \
		exit 1; \
	fi
	@echo "building serial echo test for ESP32-S3..."
	$(PIO) run -e esp32-s3-serial-echo
	@echo "uploading serial echo test to ESP32-S3..."
	$(PIO) run -e esp32-s3-serial-echo -t upload
	@echo "(: serial echo test uploaded"
	@echo "(: waiting 2 seconds for device to reset..."
	@sleep 2
	@echo "(: opening serial monitor (ctrl+c to exit)..."
	@echo "(: NOTE: Type characters in the serial monitor to see them echoed back"
	$(PIO) device monitor -e esp32-s3-serial-echo --baud 115200

test_tft_restore:	# uneeded, just feel safer with this for some reason, remove later
	@if [ -f "src/platform/esp32/main_esp32.cpp.bak" ]; then \
		mv src/platform/esp32/main_esp32.cpp.bak src/platform/esp32/main_esp32.cpp; \
		echo "(: original main_esp32.cpp restored"; \
	else \
		echo "no backup found, nothing to restore"; \
	fi

# alternative: direct esptool.py upload (if .bin file)
.PHONY: esp32-upload-direct
ESP32_PORT ?= /dev/ttyUSB0
ESP32_BAUD ?= 921600

esp32-upload-direct:
	@if [ -z "$(shell command -v esptool.py 2> /dev/null)" ]; then \
		echo "error: esptool.py not found...."; \
		exit 1; \
	fi
	@if [ ! -f "$(BUILD_DIR)/firmware.bin" ]; then \
		echo "error: firmware.bin not found. you have to build first with: make esp32-build"; \
		exit 1; \
	fi
	@echo "uploading to ESP32-S3 on $(ESP32_PORT)..."
	esptool.py --chip esp32s3 --port $(ESP32_PORT) --baud $(ESP32_BAUD) \
		write_flash -z 0x0 $(BUILD_DIR)/firmware.bin
	@echo "(: upload complete"

.PHONY: help
help:
	@echo "available targets:"
	@echo "  make              - compile and link binary (PC)"
	@echo "  make run          - run it (PC)"
	@echo "  make test         - run all tests"
	@echo "  make rebuild      - clean and rebuild everything"
	@echo "  make clean        - remove build directory"
	@echo ""
	@echo "available test apps:"
	@for test in $(TEST_NAMES); do \
		echo "  - $$test"; \
	done
	@echo ""
	@echo "ESP32 targets (requires PlatformIO):"
	@echo "  make esp32-build  - build for ESP32-S3"
	@echo "  make esp32-upload - build and upload to ESP32-S3"
	@echo "  make esp32-monitor - open serial monitor"
	@echo "  make esp32-clean  - clean ESP32 build"
	@echo "  make esp32-all    - build and upload in one step"
	@echo "  make esp32-small  - build, upload, and open serial monitor (for TTY input)"
	@echo ""
	@echo "  make esp32-upload-direct - upload using esptool.py directly"
	@echo "    (set ESP32_PORT=/dev/ttyUSB0 and ESP32_BAUD=921600 if needed)"
	@echo ""
	@echo "Filesystem utility targets (requires PlatformIO):"
	@echo "  make filesystem-build  - build filesystem initialization utility"
	@echo "  make filesystem-upload - upload all upload utilities (in upload/, not deprecated/)"
	@echo ""
	@echo "File read test target (requires PlatformIO):"
	@echo "  make file-read    - build, upload, and monitor file read test"
	@echo ""
	@echo "  make help         - show this message"
