AS       := nasm
CC       := gcc
ASFLAGS  := -f elf64 -g -F dwarf
CFLAGS   := -Wall -Wextra -g -no-pie
LDFLAGS  := 

TASK     ?= my_printf
TARGET   := $(TASK)_bin

SRC_DIR  := ./$(TASK)
OBJ_DIR  := ./obj/$(TASK)

AS_SOURCES := $(wildcard $(SRC_DIR)/*.s)
C_SOURCES  := $(wildcard $(SRC_DIR)/*.c)

OBJECTS    := $(patsubst $(SRC_DIR)/%.s, $(OBJ_DIR)/%.o, $(AS_SOURCES)) \
              $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SOURCES))

.PHONY: all clean run debug disasm

all: $(OBJ_DIR) $(TARGET)

$(TARGET): $(OBJECTS)
	@if [ -z "$(OBJECTS)" ]; then echo "Error: No files to build!"; exit 1; fi
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "--- Build '$(TASK)' finished! ---"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

disasm: all
	objdump -d -M intel $(TARGET)

clean:
	rm -rf ./obj $(TARGET)

run: all
	./$(TARGET)

debug: all
	gdb ./$(TARGET)