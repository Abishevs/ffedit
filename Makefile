# Variables
CC := gcc
CFLAGS := -Wall -Wextra -lncurses
SRC_DIR := src
BUILD_DIR := build
TARGET := main 
INSTALL_DIR := $(HOME)/.local/bin

# Automatically find all .c and .h files in src/
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default rule
all: $(BUILD_DIR)/$(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile each .c file to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link all .o files into the final binary
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Install the binary to ~/.local/bin
install: $(BUILD_DIR)/$(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(TARGET) $(INSTALL_DIR)/$(TARGET)

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

# Uninstall the binary from ~/.local/bin
uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET)

# Phony targets
.PHONY: all install clean uninstall

