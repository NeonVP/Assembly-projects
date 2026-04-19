CC = gcc
CFLAGS = -Wall -Wextra -O3 -Iinc

SRC_FILES = $(wildcard src/HashTableV*.c)
BINS = $(patsubst src/HashTable%.c, hash_%, $(SRC_FILES))

.PHONY: all clean

all: gen $(BINS)

gen: tools/Gen.c
	$(CC) $(CFLAGS) $< -o $@

hash_%: src/main.c src/HashTable%.c
	$(CC) $(CFLAGS) $^ -o $@

data.txt: gen
	@echo "Генерация тестовых данных (1.5 млн строк)..."
	./gen 1500000 > data.txt
	@echo "Готово! Данные сохранены в data.txt"

clean:
	rm -f gen hash_* data.txt
