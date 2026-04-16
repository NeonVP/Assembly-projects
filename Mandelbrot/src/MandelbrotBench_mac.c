#include "Mandelbrot.h"

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256

typedef struct BenchResult {
    MandelbrotImpl impl;
    uint64_t avg_ticks;
    uint64_t best_ticks;
    uint64_t checksum;
    double avg_ms;
    double best_ms;
} BenchResult;

static void bind_to_p_core( void ) {
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );

    thread_affinity_policy_data_t policy = { 1 };
    thread_policy_set( mach_thread_self(), THREAD_AFFINITY_POLICY, ( thread_policy_t )&policy,
                       THREAD_AFFINITY_POLICY_COUNT );
}

static uint64_t now_ticks( void ) {
    return mach_absolute_time();
}

static const char *timer_unit( void ) {
    return "ticks";
}

static double ticks_to_ms( uint64_t ticks ) {
    static mach_timebase_info_data_t tb = { 0, 0 };
    if ( tb.denom == 0 )
        mach_timebase_info( &tb );

    double ns = ( double )ticks * ( double )tb.numer / ( double )tb.denom;
    return ns / 1e6;
}

static uint64_t checksum_img( const unsigned char *img, size_t n ) {
    uint64_t h = 1469598103934665603ull;
    for ( size_t i = 0; i < n; i++ ) {
        h ^= img[i];
        h *= 1099511628211ull;
    }
    return h;
}

static BenchResult run_case( MandelbrotImpl impl, unsigned char *buffer, int runs, int warmup,
                             float xmin, float xmax, float ymin, float ymax ) {
    bind_to_p_core();
    BenchResult r = { .impl = impl };

    for ( int i = 0; i < warmup; i++ )
        mandelbrot_compute( buffer, WIDTH, HEIGHT, MAX_ITER, xmin, xmax, ymin, ymax, impl );

    uint64_t total_ticks = 0;
    uint64_t best_ticks = UINT64_MAX;

    for ( int i = 0; i < runs; i++ ) {
        uint64_t t0 = now_ticks();
        mandelbrot_compute( buffer, WIDTH, HEIGHT, MAX_ITER, xmin, xmax, ymin, ymax, impl );
        uint64_t dt = now_ticks() - t0;
        total_ticks += dt;
        if ( dt < best_ticks )
            best_ticks = dt;
    }

    r.avg_ticks = total_ticks / ( uint64_t )runs;
    r.best_ticks = best_ticks;
    r.checksum = checksum_img( buffer, ( size_t )WIDTH * HEIGHT );
    r.avg_ms = ticks_to_ms( r.avg_ticks );
    r.best_ms = ticks_to_ms( r.best_ticks );

    printf( "%-10s avg: %8.3f ms | best: %8.3f ms | checksum: %016llx\n",
            mandelbrot_impl_name( impl ), r.avg_ms, r.best_ms, ( unsigned long long )r.checksum );

    return r;
}

static void write_csv( const char *path, const BenchResult *results, int count, int runs,
                       int warmup ) {
    FILE *f = fopen( path, "w" );
    if ( !f ) {
        fprintf( stderr, "failed to open csv file: %s\n", path );
        return;
    }

    fprintf( f, "impl,available,avg_ticks,best_ticks,avg_ms,best_ms,checksum,runs,warmup,width,"
                "height,max_iter,timer_unit\n" );

    for ( int i = 0; i < count; i++ ) {
        const BenchResult *r = &results[i];
        fprintf( f, "%s,%d,%llu,%llu,%.6f,%.6f,%016llx,%d,%d,%d,%d,%d,%s\n",
                 mandelbrot_impl_name( r->impl ), 1, ( unsigned long long )r->avg_ticks,
                 ( unsigned long long )r->best_ticks, r->avg_ms, r->best_ms,
                 ( unsigned long long )r->checksum, runs, warmup, WIDTH, HEIGHT, MAX_ITER,
                 timer_unit() );
    }

    fclose( f );
    printf( "CSV saved: %s\n", path );
}

int main( int argc, char **argv ) {
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );

    int runs = 1000;
    int warmup = 25;
    const char *csv_path = NULL;

    if ( argc > 1 )
        runs = atoi( argv[1] );
    if ( argc > 2 )
        warmup = atoi( argv[2] );
    if ( argc > 3 )
        csv_path = argv[3];

    if ( runs <= 0 )
        runs = 30;
    if ( warmup < 0 )
        warmup = 5;

    unsigned char *buffer = malloc( ( size_t )WIDTH * HEIGHT );
    if ( !buffer ) {
        fprintf( stderr, "allocation failed\n" );
        return 1;
    }

    float xmin = -2.2f, xmax = 1.0f, ymin = -1.2f, ymax = 1.2f;
    MandelbrotImpl impls[] = { MANDEL_IMPL_V1,
                               MANDEL_IMPL_V2,
                               MANDEL_IMPL_V3_NEON8,
                               MANDEL_IMPL_V3_NEON4,
                               MANDEL_IMPL_V3_NEON16 };
    const int impl_count = ( int )( sizeof( impls ) / sizeof( impls[0] ) );
    BenchResult results[5] = { 0 };

    printf( "Mandelbrot mac benchmark | %dx%d | MAX_ITER=%d | runs=%d warmup=%d\n", WIDTH,
            HEIGHT, MAX_ITER, runs, warmup );

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

    if ( csv_path && csv_path[0] != '\0' )
        write_csv( csv_path, results, impl_count, runs, warmup );

    free( buffer );
    return 0;
}
