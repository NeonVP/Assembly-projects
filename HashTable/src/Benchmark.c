#include "Benchmark.h"
#include "HashTable.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>
#include <cpuid.h>

struct TestContext {
    HashTable *table;
    char ( *inserted_words )[MAX_LINE_LENGTH]; 
    int inserted_count;
    char ( *queries )[MAX_LINE_LENGTH];        
    int query_count;
};

static inline uint64_t get_start_cycles() {
    unsigned int a, b, c, d;
    __cpuid( 1, a, b, c, d ); 
    return __rdtsc();
}

static inline uint64_t get_end_cycles() {
    unsigned int aux;
    uint64_t cycles = __rdtscp( &aux );
    unsigned int a, b, c, d;
    __cpuid( 1, a, b, c, d );
    return cycles;
}

static size_t CalcInitialCapacity( size_t element_count, double target_load_factor ) {
    if ( element_count == 0 ) return 1;
    size_t cap = ( size_t )( ( double )element_count / target_load_factor );
    if ( ( double )cap * target_load_factor < ( double )element_count ) cap++;
    return cap > 0 ? cap : 1;
}

TestContext *TestContextCreate( int words_count, int queries_count, double load_factor ) {
    TestContext *ctx = ( TestContext * )calloc( 1, sizeof( TestContext ) );
    assert( ctx && "Failed to allocate TestContext" );

    ctx->inserted_count = words_count;
    ctx->query_count = queries_count;

    ctx->inserted_words = calloc( words_count, MAX_LINE_LENGTH );
    ctx->queries = calloc( queries_count, MAX_LINE_LENGTH );

    size_t initial_capacity = CalcInitialCapacity( words_count, load_factor );
    
    ctx->table = HashTableCtor( initial_capacity, load_factor );
    assert( ctx->table && "Failed to create hash table" );

    return ctx;
}

void TestContextLoadData( TestContext *ctx ) {
    char buffer[MAX_LINE_LENGTH] = {};
    for ( int i = 0; i < ctx->inserted_count; i++ ) {
        if ( scanf( "%31s", buffer ) != 1 ) break;

        memcpy( ctx->inserted_words[i], buffer, MAX_LINE_LENGTH );
        HashTableInsert( ctx->table, ctx->inserted_words[i] );
    }
}

void TestContextGenerateQueries( TestContext *ctx ) {
    printf( "Preparing %d requests...\n", ctx->query_count );

    for ( int i = 0; i < ctx->query_count; i++ ) {
        int random_index = rand() % ctx->inserted_count;
        
        memcpy( ctx->queries[i], ctx->inserted_words[random_index], MAX_LINE_LENGTH );

        if ( i % 2 != 0 ) {
            size_t len = strlen( ctx->queries[i] );
            if ( len < MAX_LINE_LENGTH - 1 ) {
                ctx->queries[i][len] = 'Z'; 
                ctx->queries[i][len + 1] = '\0';
            } else {
                ctx->queries[i][MAX_LINE_LENGTH - 2] = 'Z';
                ctx->queries[i][MAX_LINE_LENGTH - 1] = '\0';
            }
        }
    }
}

void TestContextRunWarmup( TestContext *ctx, int warmup_count ) {
    if ( warmup_count <= 0 ) return;
    
    printf( "Warming up (%d searches)...\n", warmup_count );
    
    volatile uint64_t dump = 0; 
    for ( int i = 0; i < warmup_count; i++ ) {
        dump += HashTableContains( ctx->table, ctx->queries[i] );
    }
}

void TestContextRunBenchmark( TestContext *ctx, int num_runs, int num_searches ) {
    printf( "Starting the search measurement (%d runs, %d searches per run)...\n", num_runs, num_searches );

    uint64_t ticks_per_run[num_runs];

    for ( int run = 0; run < num_runs; run++ ) {
        volatile uint64_t found_count = 0;
        uint64_t start_ticks = get_start_cycles();

        for ( int i = 0; i < num_searches; i++ ) {
            found_count += HashTableContains( ctx->table, ctx->queries[i] );
        }

        uint64_t end_ticks = get_end_cycles();
        uint64_t ticks = end_ticks - start_ticks;
        ticks_per_run[run] = ticks;

        printf( "[Run %d] Найдено: %llu | Тактов: %llu | На поиск: %llu\n", 
                run + 1, ( unsigned long long )found_count, 
                ( unsigned long long )ticks, ( unsigned long long )( ticks / num_searches ) );
    }

    if ( num_runs > 2 ) {
        uint64_t min_ticks = ticks_per_run[0], max_ticks = ticks_per_run[0], sum_ticks = 0;
        for ( int i = 0; i < num_runs; i++ ) {
            if ( ticks_per_run[i] < min_ticks ) min_ticks = ticks_per_run[i];
            if ( ticks_per_run[i] > max_ticks ) max_ticks = ticks_per_run[i];
            sum_ticks += ticks_per_run[i];
        }

        uint64_t valid_runs = num_runs - 2;
        uint64_t filtered_sum = sum_ticks - min_ticks - max_ticks;
        uint64_t avg_ticks_per_run = filtered_sum / valid_runs;
        uint64_t avg_ticks_per_search = avg_ticks_per_run / num_searches;

        printf( "\n--- Итоговая статистика ---\n" );
        printf( "Среднее число тактов на запуск: %llu\n", ( unsigned long long )avg_ticks_per_run );
        printf( "Среднее число тактов на 1 поиск: %llu\n", ( unsigned long long )avg_ticks_per_search );
    }
}

void TestContextDestroy( TestContext *ctx ) {
    if ( !ctx ) return;
    HashTableDtor( ctx->table );
    free( ctx->queries );
    free( ctx->inserted_words );
    free( ctx );
}
