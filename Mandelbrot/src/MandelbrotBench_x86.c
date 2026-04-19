#include "Mandelbrot.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <x86intrin.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256

typedef struct BenchResult {
    MandelbrotImpl impl;
    uint64_t avg_ticks;
    uint64_t best_ticks;
    double rms_ticks;
    double rel_rms_percent;
    uint64_t checksum;
    double avg_ms;
    double best_ms;
} BenchResult;

static uint64_t now_ticks( void ) {
    _mm_lfence(); 
    uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}

static BenchResult run_case( MandelbrotImpl impl, unsigned char *buffer, int runs, int warmup,
                             float xmin, float xmax, float ymin, float ymax ) {
    BenchResult r = { .impl = impl };
    uint64_t checksum = 0;

    for ( int i = 0; i < warmup; i++ )
        checksum = mandelbrot_bench_compute( buffer, WIDTH, HEIGHT, MAX_ITER, xmin, xmax, ymin, ymax, impl );

    uint64_t best_ticks = UINT64_MAX;
    uint64_t worst_ticks = 0;
    long double sum_ticks = 0.0L;
    long double sumsq_ticks = 0.0L;

    for ( int i = 0; i < runs; i++ ) {
        uint64_t t0 = now_ticks();
        checksum = mandelbrot_bench_compute( buffer, WIDTH, HEIGHT, MAX_ITER, xmin, xmax, ymin, ymax, impl );
        uint64_t dt = now_ticks() - t0;
        sum_ticks += ( long double )dt;
        sumsq_ticks += ( long double )dt * ( long double )dt;
        if ( dt < best_ticks ) best_ticks = dt;
        if ( dt > worst_ticks ) worst_ticks = dt;
    }

    int effective_runs = runs;
    long double eff_sum = sum_ticks;
    long double eff_sumsq = sumsq_ticks;
    if ( runs > 2 ) {
        effective_runs = runs - 2;
        eff_sum -= ( long double )best_ticks + ( long double )worst_ticks;
        eff_sumsq -= ( long double )best_ticks * ( long double )best_ticks +
                     ( long double )worst_ticks * ( long double )worst_ticks;
    }

    long double mean = eff_sum / ( long double )effective_runs;
    long double variance = eff_sumsq / ( long double )effective_runs - mean * mean;
    if ( variance < 0.0L ) variance = 0.0L;

    r.avg_ticks = ( uint64_t )( mean + 0.5L );
    r.best_ticks = best_ticks;
    r.rms_ticks = sqrt( ( double )variance );
    r.rel_rms_percent = ( mean > 0.0L ) ? ( r.rms_ticks / ( double )mean ) * 100.0 : 0.0;
    r.checksum = checksum;
    r.avg_ms = -1.0;  // Заглушка для совместимости с питон скриптом (он поймет, что это тики)
    r.best_ms = -1.0; 

    printf( "%-10s avg: %12llu cycles | rms: %10.2f | rel: %6.2f%% | checksum: %016llx\n",
            mandelbrot_impl_name( impl ), ( unsigned long long )r.avg_ticks,
            r.rms_ticks, r.rel_rms_percent,
            ( unsigned long long )r.checksum );

    return r;
}

static void write_csv( const char *path, const BenchResult *results, int count, int runs, int warmup ) {
    FILE *f = fopen( path, "w" );
    if ( !f ) {
        fprintf( stderr, "failed to open csv file: %s\n", path );
        return;
    }

    fprintf( f, "impl,available,avg_ticks,best_ticks,rms_ticks,rel_rms_percent,avg_ms,best_ms,checksum,runs,warmup,width,height,max_iter,timer_unit\n" );

    for ( int i = 0; i < count; i++ ) {
        const BenchResult *r = &results[i];
        fprintf( f, "%s,%d,%llu,%llu,%.6f,%.6f,%.6f,%.6f,%016llx,%d,%d,%d,%d,%d,cycles\n",
                 mandelbrot_impl_name( r->impl ), 1, ( unsigned long long )r->avg_ticks,
                 ( unsigned long long )r->best_ticks, r->rms_ticks, r->rel_rms_percent,
                 r->avg_ms, r->best_ms,
                 ( unsigned long long )r->checksum, runs, warmup, WIDTH, HEIGHT, MAX_ITER );
    }

    fclose( f );
    printf( "CSV saved: %s\n", path );
}

int main( int argc, char **argv ) {
    int runs = ( argc > 1 ) ? atoi( argv[1] ) : 1000;
    int warmup = ( argc > 2 ) ? atoi( argv[2] ) : 25;
    const char *csv_path = ( argc > 3 ) ? argv[3] : NULL;

    unsigned char *buffer = malloc( ( size_t )WIDTH * HEIGHT );
    if ( !buffer ) {
        fprintf( stderr, "allocation failed\n" );
        return 1;
    }
    float xmin = -2.2f, xmax = 1.0f, ymin = -1.2f, ymax = 1.2f;
    
    MandelbrotImpl impls[] = { MANDEL_IMPL_V1, MANDEL_IMPL_V2, MANDEL_IMPL_V3_X86, MANDEL_IMPL_V3_X86_AVX512 };
    const int impl_count = ( int )( sizeof( impls ) / sizeof( impls[0] ) );
    BenchResult results[4] = { 0 };

    printf( "Mandelbrot x86 benchmark | %dx%d | MAX_ITER=%d | runs=%d warmup=%d\n",
            WIDTH, HEIGHT, MAX_ITER, runs, warmup );

    uint64_t base_avg_ticks = 0;
    for ( int i = 0; i < impl_count; i++ ) {
        results[i] = run_case( impls[i], buffer, runs, warmup, xmin, xmax, ymin, ymax );

        if ( impls[i] == MANDEL_IMPL_V1 ) {
            base_avg_ticks = results[i].avg_ticks;
        } else if ( base_avg_ticks > 0 ) {
            double speedup = ( double )base_avg_ticks / ( double )results[i].avg_ticks;
            printf( "           speedup vs v1: x%.2f\n", speedup );
        }
    }

    if ( csv_path && csv_path[0] != '\0' ) write_csv( csv_path, results, impl_count, runs, warmup );
    free( buffer );
    return 0;
}
