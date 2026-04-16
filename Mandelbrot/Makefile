CC := gcc
BASE_CFLAGS := -Wall -Wextra -O3

RAYLIB_LIBS_COMMON := -lraylib -lm -lpthread -ldl

ifeq ($(UNAME_S),Darwin)
    # macOS
    RAYLIB_CFLAGS := -I/opt/homebrew/opt/raylib/include
    RAYLIB_LIBS := -L/opt/homebrew/opt/raylib/lib $(RAYLIB_LIBS_COMMON)
else
    # Linux (обычно стандартные)
    RAYLIB_CFLAGS := 
    RAYLIB_LIBS := $(RAYLIB_LIBS_COMMON)
endif

SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BENCH_CSV_DIR := bench_results

V1_BIN := MandelbrotV1 
V2_BIN := MandelbrotV2
V3NEON4_BIN := MandelbrotV3neon4
V3NEON8_BIN := MandelbrotV3neon8
V3NEON16_BIN := MandelbrotV3neon16
V3NEON32_BIN := MandelbrotV3neon32
V3X86_BIN := MandelbrotV3x86
V3X86_AVX512_BIN := MandelbrotV3x86_avx512

BENCH_X86_BIN := MandelbrotBench_x86
BENCH_MAC_BIN := MandelbrotBench_mac

BENCH_RUNS ?= 1000
BENCH_WARMUP ?= 25

.PHONY: all clean bench bench_mac variants v1 v2 v3neon8 v3neon4 v3neon16 v3neon32 v3x86 v3x86_avx512

all: variants

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

v1: $(V1_BIN)
$(V1_BIN): $(SRC_DIR)/MandelbrotV1.c
	$(CC) $(BASE_CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

v2: $(V2_BIN)
$(V2_BIN): $(SRC_DIR)/MandelbrotV2.c
	$(CC) $(BASE_CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

# NEON
v3neon4: $(V3NEON4_BIN)
$(V3NEON4_BIN): $(SRC_DIR)/MandelbrotV3neon4.c
	$(CC) $(BASE_CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

v3neon8: $(V3NEON8_BIN)
$(V3NEON8_BIN): $(SRC_DIR)/MandelbrotV3neon8.c
	$(CC) $(BASE_CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

v3neon16: $(V3NEON16_BIN)
$(V3NEON16_BIN): $(SRC_DIR)/MandelbrotV3neon16.c
	$(CC) $(BASE_CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

v3neon32: $(V3NEON32_BIN)
$(V3NEON32_BIN): $(SRC_DIR)/MandelbrotV3neon32.c
	$(CC) $(BASE_CFLAGS) $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

# x86_64
v3x86: $(V3X86_BIN)
$(V3X86_BIN): $(SRC_DIR)/MandelbrotV3x86.c
	$(CC) $(BASE_CFLAGS) -mavx $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

v3x86_avx512: $(V3X86_AVX512_BIN)
$(V3X86_AVX512_BIN): $(SRC_DIR)/MandelbrotV3x86_avx512.c
	$(CC) $(BASE_CFLAGS) -mavx512f $(RAYLIB_CFLAGS) -I$(INC_DIR) $< $(RAYLIB_LIBS) -o $@

variants: v1 v2 v3neon8 v3neon4 v3neon16 v3neon32 v3x86 v3x86_avx512


# Benchmarks
# x86
$(OBJ_DIR)/MandelbrotBench.o: $(SRC_DIR)/MandelbrotBench.c | $(OBJ_DIR)
	$(CC) $(BASE_CFLAGS) -mavx -mavx512f -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR)/MandelbrotFn.o: $(SRC_DIR)/MandelbrotFn.c | $(OBJ_DIR)
	$(CC) $(BASE_CFLAGS) -mavx -mavx512f -I$(INC_DIR) -c $< -o $@

$(BENCH_X86_BIN): $(OBJ_DIR)/MandelbrotBench.o $(OBJ_DIR)/MandelbrotFn.o
	$(CC) $^ -lm -lpthread -o $@

bench: $(BENCH_X86_BIN)
	./$(BENCH_X86_BIN) $(BENCH_RUNS) $(BENCH_WARMUP)

# NEON
$(OBJ_DIR)/MandelbrotBench_mac.o: $(SRC_DIR)/MandelbrotBench_mac.c | $(OBJ_DIR)
	$(CC) $(BASE_CFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR)/MandelbrotFn_mac.o: $(SRC_DIR)/MandelbrotFn.c | $(OBJ_DIR)
	$(CC) $(BASE_CFLAGS) -I$(INC_DIR) -c $< -o $@

$(BENCH_MAC_BIN): $(OBJ_DIR)/MandelbrotBench_mac.o $(OBJ_DIR)/MandelbrotFn_mac.o
	$(CC) $^ -lm -lpthread -o $@

bench_mac: $(BENCH_MAC_BIN)
	./$(BENCH_MAC_BIN) $(BENCH_RUNS) $(BENCH_WARMUP)


clean:
	rm -rf $(OBJ_DIR) $(BENCH_X86_BIN) $(BENCH_MAC_BIN) \
		$(V1_BIN) $(V2_BIN) $(V3NEON8_BIN) $(V3NEON4_BIN) \
		$(V3NEON16_BIN) $(V3NEON32_BIN) $(V3X86_BIN) $(V3X86_AVX512_BIN)
