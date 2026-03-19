AS       := nasm
LD       := ld
ASFLAGS  := -f elf32 -g -F dwarf -w+all
LDFLAGS  := -m elf_i386

TASK     ?= my_prinf

TARGET   := $(TASK)_bin

SRC_DIR  := ./$(TASK)
OBJ_DIR  := ./obj/$(TASK)

SOURCES  := $(wildcard $(SRC_DIR)/*.s)
OBJECTS  := $(patsubst $(SRC_DIR)/%.s, $(OBJ_DIR)/%.o, $(SOURCES))


.PHONY: all clean run debug

all: $(OBJ_DIR) $(TARGET)

$(TARGET): $(OBJECTS)
	@if [ -z "$(SOURCES)" ]; then echo "Ошибка: В папке $(SRC_DIR) нет .s файлов!"; exit 1; fi
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "--- Сборка '$(TASK)' завершена! ---"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)


clean:
	rm -rf ./obj $(TARGET)

run: all
	./$(TARGET)

debug: all
	gdb ./$(TARGET)