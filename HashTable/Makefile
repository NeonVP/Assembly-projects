CC = gcc
CFLAGS = -Wall -Wextra -O3 -Iinc -g -march=native -masm=intel -mavx2

COMMON_SRC = src/Benchmark.c src/CFList.c

TABLE_FILES = $(wildcard src/HashTableV*.c)
BINS = $(patsubst src/HashTable%.c, hash_%, $(TABLE_FILES))

SEARCHES ?= 5000000
RUNS     ?= 5
WARMUPS  ?= 500000  

HASH_V0 ?= crc32
HASH_V1 ?= crc32intrin
HASH_V2 ?= crc32intrin
HASH_V3 ?= crc32intrin

.PHONY: all clean run_% perf_%

all: $(BINS)

hash_V3: src/main.c src/HashTableV3.c src/contains.s $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@

hash_%: src/main.c src/HashTable%.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@

CURRENT_HASH = $(if $(HASH_$*),$(HASH_$*),crc32)

run_%: hash_% 
	@echo "\n========================================"
	@echo " Запуск $* без Perf "
	@echo "========================================"
	./$< -s $(SEARCHES) -r $(RUNS) -w $(WARMUPS) < data.txt

perf_%: hash_%
	@echo "\n========================================"
	@echo " Запуск $* с Perf "
	@echo "========================================"
	sudo perf record -g nice -n -20 taskset -c 3 ./$< -s $(SEARCHES) -r $(RUNS) -w $(WARMUPS) < data.txt

clean:
	rm -f hash_* perf.data*