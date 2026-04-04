CC := gcc
CFLAGS := -Wall -Wextra -g -O3
RAYLIB_CFLAGS := -I/opt/homebrew/opt/raylib/include
RAYLIB_LIBS := -L/opt/homebrew/opt/raylib/lib -lraylib -lm -lpthread -ldl

SRC_DIR := ./src
INC_DIR := ./include
OBJ_DIR := ./obj
BIN := mandelbrot_bin

SOURCES := $(SRC_DIR)/main.c $(SRC_DIR)/MandelbrotFn.c
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

.PHONY: all clean run

all: $(OBJ_DIR) $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(OBJECTS) $(RAYLIB_LIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: all
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN)
