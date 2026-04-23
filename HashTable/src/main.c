#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <x86intrin.h>
#include <cpuid.h>

#include "HashTable.h"

#define MAX_LINE_LENGTH 256
#define TARGET_LOAD_FACTOR 10.0
#define NUM_SEARCHES 5000000
#define NUM_RUNS 5

static inline uint64_t get_start_cycles() {
    unsigned int a = 0, b = 0, c = 0, d = 0;
    __cpuid( 1, a, b, c, d ); 
    return __rdtsc();
}

static inline uint64_t get_end_cycles() {
    unsigned int aux = 0;
    uint64_t cycles = __rdtscp( &aux );

    unsigned int a = 0, b = 0, c = 0, d = 0;
    __cpuid( 1, a, b, c, d );
    return cycles;
}

typedef struct {
    HashTableChaining *table;
    StringKey *inserted_words;
    int inserted_count;
    StringKey *queries;
    int query_count;
} TestContext;

TestContext *TestContextCreate( int words_count, int queries_count, double load_factor ) {
    TestContext *ctx = ( TestContext * )calloc( 1, sizeof( TestContext ) );
    assert( ctx && "Failed to allocate TestContext" );

    ctx->inserted_count = words_count;
    ctx->query_count = queries_count;

    ctx->inserted_words = ( StringKey * )calloc( words_count, sizeof( StringKey ) );
    assert( ctx->inserted_words && "Failed to allocate inserted_words" );

    ctx->queries = ( StringKey * )calloc( queries_count, sizeof( StringKey ) );
    assert( ctx->queries && "Failed to allocate queries" );

    size_t initial_capacity = HashTableCapacityForLoadFactor( ( size_t )words_count, load_factor );
    ctx->table = HashTableChainingCtor( initial_capacity, load_factor );
    assert( ctx->table && "Failed to create hash table" );

    return ctx;
}

void TestContextLoadData( TestContext *ctx ) {
    char buffer[MAX_LINE_LENGTH] = {};
    for ( int i = 0; i < ctx->inserted_count; i++ ) {
        if ( scanf( "%255s", buffer ) != 1 ) {
            break;
        }

        size_t len = strlen( buffer );
        char *str_copy = ( char * )calloc( len + 1, sizeof( char ) );
        assert( str_copy && "Failed to allocate string copy" );
        
        strcpy( str_copy, buffer );

        ctx->inserted_words[i].data = str_copy;
        ctx->inserted_words[i].len = len;

        HashTableChainingInsert( ctx->table, ctx->inserted_words[i] );
    }
}

void TestContextGenerateQueries( TestContext *ctx ) {
    printf( "Preparing %d requests...\n", ctx->query_count );

    for ( int i = 0; i < ctx->query_count; i++ ) {
        int random_index = rand() % ctx->inserted_count;
        StringKey base_word = ctx->inserted_words[random_index];

        if ( i % 2 == 0 ) {
            ctx->queries[i] = base_word;
        } else {
            char *bad_word = ( char * )calloc( base_word.len + 2, sizeof( char ) );
            assert( bad_word && "Failed to allocate bad word" );

            strcpy( bad_word, base_word.data );
            bad_word[base_word.len] = 'Z'; 
            bad_word[base_word.len + 1] = '\0';
            
            ctx->queries[i].data = bad_word;
            ctx->queries[i].len = base_word.len + 1;
        }
    }
}

void TestContextRunBenchmark( TestContext *ctx, int num_runs ) {
    printf( "Starting the search measurement (%d runs)...\n", num_runs );

    uint64_t ticks_per_run[num_runs];

    for ( int run = 0; run < num_runs; run++ ) {
        volatile uint64_t found_count = 0;
        uint64_t start_ticks = get_start_cycles();

        for ( int i = 0; i < ctx->query_count; i++ ) {
            found_count += HashTableChainingContains( ctx->table, ctx->queries[i] );
        }

        uint64_t end_ticks = get_end_cycles();
        uint64_t ticks = end_ticks - start_ticks;
        ticks_per_run[run] = ticks;

        printf( "[Run %d] Найдено: %llu | Тактов: %llu | На поиск: %llu\n", 
                run + 1,
                ( unsigned long long )found_count,
                ( unsigned long long )ticks,
                ( unsigned long long )( ticks / ctx->query_count ) );
    }

    if ( num_runs > 2 ) {
        uint64_t min_ticks = ticks_per_run[0];
        uint64_t max_ticks = ticks_per_run[0];
        uint64_t sum_ticks = 0;

        for ( int i = 0; i < num_runs; i++ ) {
            if ( ticks_per_run[i] < min_ticks ) {
                min_ticks = ticks_per_run[i];
            }
            if ( ticks_per_run[i] > max_ticks ) {
                max_ticks = ticks_per_run[i];
            }
            sum_ticks += ticks_per_run[i];
        }

        uint64_t valid_runs = num_runs - 2;
        uint64_t filtered_sum = sum_ticks - min_ticks - max_ticks;
        uint64_t avg_ticks_per_run = filtered_sum / valid_runs;
        uint64_t avg_ticks_per_search = avg_ticks_per_run / ctx->query_count;

        printf( "\n--- Итоговая статистика ---\n" );
        printf( "Отброшены: max = %llu, min = %llu\n", 
                ( unsigned long long )max_ticks, 
                ( unsigned long long )min_ticks );
        printf( "Среднее число тактов на запуск (по %llu валидным прогонам): %llu\n", 
                ( unsigned long long )valid_runs, 
                ( unsigned long long )avg_ticks_per_run );
        printf( "Среднее число тактов на 1 поиск: %llu\n", 
                ( unsigned long long )avg_ticks_per_search );
    }
}

void TestContextDestroy( TestContext *ctx ) {
    if ( !ctx ) {
        return;
    }

    HashTableChainingDtor( ctx->table );

    for ( int i = 0; i < ctx->query_count; i++ ) {
        if ( i % 2 != 0 && ctx->queries[i].data ) {
            free( ( void * )ctx->queries[i].data );
        }
    }
    free( ctx->queries );

    for ( int i = 0; i < ctx->inserted_count; i++ ) {
        if ( ctx->inserted_words[i].data ) {
            free( ( void * )ctx->inserted_words[i].data );
        }
    }
    free( ctx->inserted_words );

    free( ctx );
}

int main() {
    int n = 0;
    if ( scanf( "%d", &n ) != 1 ) {
        fprintf( stderr, "Ошибка: не удалось считать количество строк (n).\n" );
        return 1;
    }

    TestContext *ctx = TestContextCreate( n, NUM_SEARCHES, TARGET_LOAD_FACTOR );
    
    TestContextLoadData( ctx );
    TestContextGenerateQueries( ctx );
    TestContextRunBenchmark( ctx, NUM_RUNS );
    TestContextDestroy( ctx );

    return 0;
}
