#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 

#include "Benchmark.h"

void PrintHelp( const char *prog_name ) {
    printf( "Использование: %s [ОПЦИИ] < data.txt\n", prog_name );
    printf( "Опции:\n" );
    printf( "  -s <число>   Количество поисков (по умолчанию: 5000000)\n" );
    printf( "  -r <число>   Количество запусков/runs (по умолчанию: 5)\n" );
    printf( "  -w <число>   Количество прогревных поисков (по умолчанию: 0)\n" );
    printf( "  -h           Показать эту справку\n" );
}

int main( int argc, char *argv[] ) {
    int searches = 5000000;
    int runs = 5;
    int warmups = 0;

    int opt = 0;
    while ( ( opt = getopt( argc, argv, "s:r:w:f:h" ) ) != -1 ) {
        switch ( opt ) {
            case 's': searches = atoi( optarg ); break;
            case 'r': runs = atoi( optarg ); break;
            case 'w': warmups = atoi( optarg ); break;

            case 'h':
            default:
                PrintHelp( argv[0] );
                return 0;
        }
    }

    int n = 0;
    if ( scanf( "%d", &n ) != 1 ) {
        fprintf( stderr, "Ошибка: не удалось считать количество слов словаря (n) из stdin.\n" );
        return 1;
    }

    printf( "--- Настройки бенчмарка ---\n" );
    printf( "Словарь: %d слов | Поисков: %d | Запусков: %d | Прогрев: %d\n\n", 
            n, searches, runs, warmups );

    int max_queries = ( searches > warmups ) ? searches : warmups;

    TestContext *ctx = TestContextCreate( n, max_queries, TARGET_LOAD_FACTOR );
    
    TestContextLoadData( ctx );
    TestContextGenerateQueries( ctx );
    
    TestContextRunWarmup( ctx, warmups );
    TestContextRunBenchmark( ctx, runs, searches );
    
    TestContextDestroy( ctx );

    return 0;
}