# ESP32-DOOM Makefile
# Requires: ESP-IDF environment, make (via MSYS2/Git Bash on Windows)

# Configuration
PORT ?= COM3
BAUD ?= 460800
WIFI_SSID ?= YourWiFiSSID
WIFI_PASSWORD ?= YourWiFiPassword
WAD_FILE ?= data/DOOM1_MINI.WAD

# ESP-IDF paths
IDF_PATH ?= D:/ProgramData/esp-idf-v5.3.4
ESP_TOOLS = $(IDF_PATH)/tools/tools
PYTHON = $(IDF_PATH)/tools/python_env/idf5.3_py3.11_env/Scripts/python.exe

# Tool paths
CMAKE = $(ESP_TOOLS)/cmake/3.30.2/bin/cmake.exe
NINJA = $(ESP_TOOLS)/ninja/1.12.1/ninja.exe
ESPTOOL = $(PYTHON) -m esptool

# Build directory
BUILD_DIR = build

.PHONY: all build flash monitor clean fullclean help menuconfig

# Default target
all: build

# Build with streaming enabled
build:
	@echo "=== Building ESP32-DOOM (Streaming Enabled) ==="
	@echo "WiFi SSID: $(WIFI_SSID)"
	$(CMAKE) -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE=$(IDF_PATH)/tools/cmake/toolchain-esp32.cmake \
		-DCMAKE_MAKE_PROGRAM=$(NINJA) \
		-DIDF_TARGET=esp32 \
		-DDOOM_STREAMING=ON \
		-DWIFI_SSID="$(WIFI_SSID)" \
		-DWIFI_PASSWORD="$(WIFI_PASSWORD)" \
		-B $(BUILD_DIR) -S .
	$(CMAKE) --build $(BUILD_DIR)
	@echo "=== Build Complete ==="

# Flash firmware only
flash:
	@echo "=== Flashing to $(PORT) ==="
	$(ESPTOOL) --chip esp32 -p $(PORT) -b $(BAUD) \
		write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
		0x1000 $(BUILD_DIR)/bootloader/bootloader.bin \
		0x8000 $(BUILD_DIR)/partition_table/partition-table.bin \
		0x10000 $(BUILD_DIR)/esp32-doom.bin

# Flash firmware and WAD
flash-all: flash
	@echo "=== Flashing WAD file ==="
	$(ESPTOOL) --chip esp32 -p $(PORT) -b $(BAUD) \
		write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
		0x210000 $(WAD_FILE)

# Monitor serial output
monitor:
	@echo "=== Starting Monitor on $(PORT) ==="
	$(PYTHON) -m esp_idf_monitor -p $(PORT) -b 115200 $(BUILD_DIR)/esp32-doom.elf

# Build, flash, and monitor
run: build flash-all monitor

# Clean build artifacts
clean:
	@echo "=== Cleaning build ==="
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)

# Full clean including sdkconfig
fullclean: clean
	@if exist sdkconfig del sdkconfig

# Show help
help:
	@echo "ESP32-DOOM Build System"
	@echo ""
	@echo "Usage: make [target] [VARIABLE=value]"
	@echo ""
	@echo "Targets:"
	@echo "  build      - Build firmware (default)"
	@echo "  flash      - Flash firmware only"
	@echo "  flash-all  - Flash firmware + WAD file"
	@echo "  monitor    - Start serial monitor"
	@echo "  run        - Build, flash-all, and monitor"
	@echo "  clean      - Remove build directory"
	@echo "  fullclean  - Remove build and sdkconfig"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  PORT          - Serial port (default: COM3)"
	@echo "  BAUD          - Flash baud rate (default: 460800)"
	@echo "  WIFI_SSID     - WiFi network name"
	@echo "  WIFI_PASSWORD - WiFi password"
	@echo "  WAD_FILE      - WAD file path (default: data/DOOM1_MINI.WAD)"
	@echo ""
	@echo "Example:"
	@echo "  make run WIFI_SSID=MyNetwork WIFI_PASSWORD=MyPass PORT=COM5"
