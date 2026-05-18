CC = gcc
CFLAGS = -Wall -Wextra -O3 -Iinc -g -march=haswell -masm=intel -mavx2

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

.PHONY: all clean run_% perf_% callgrind_%

all: $(BINS)

hash_V2: src/main.c src/HashTableV2.c src/my_strlen.s $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@

hash_V3: src/main.c src/HashTableV3.c src/my_strlen.s src/contains.s $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@

hash_%: src/main.c src/HashTable%.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $^ -o $@

run_%: hash_% 
	@echo "========================================"
	@echo " Запуск $* "
	@echo "========================================"
	sudo nice -n -20 taskset -c 3 ./$< -s $(SEARCHES) -r $(RUNS) -w $(WARMUPS) < data.txt

callgrind_%: hash_%
	@echo "\n========================================"
	@echo " Профилирование $* через Callgrind "
	@echo "========================================"
	valgrind --tool=callgrind --callgrind-out-file=callgrind_$*.out --dump-instr=yes --collect-jumps=yes ./$< -s 75000000 -r 1 -w 5000 < data.txt
	kcachegrind callgrind_$*.out

clean:
	rm -f hash_* perf.data*
