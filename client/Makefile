# Project name (output executable)
TARGET = client

# Directories
SRC_DIR = src
INC_DIR = inc
OBJ_DIR = obj

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -I$(INC_DIR)

# Source and object files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJ)
	@echo "🔗 Linking..."
	$(CC) $(OBJ) -o $@

# Compiling each .c file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "⚙️  Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Create obj directory if not exists
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean object and binary files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# Run the executable
run: $(TARGET)
	./$(TARGET)

