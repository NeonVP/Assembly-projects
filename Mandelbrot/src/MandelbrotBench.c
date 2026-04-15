#include "Mandelbrot.h"

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined( __APPLE__ )
#include <mach/mach_time.h>
#elif defined( __linux__ ) && defined( __x86_64__ )
#include <x86intrin.h>
#endif

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256

typedef struct BenchResult {
    MandelbrotImpl impl;
    int available;
    uint64_t avg_ticks;
    uint64_t best_ticks;
    uint64_t checksum;
    double avg_ms;
    double best_ms;
} BenchResult;


#ifdef __APPLE__
void bind_to_p_core( void ) {
    // Устанавливаем максимальный приоритет
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );

    // Подсказка планировщику для Affinity (группировка потоков)
    thread_affinity_policy_data_t policy = { 1 };
    thread_policy_set( mach_thread_self(), THREAD_AFFINITY_POLICY, ( thread_policy_t )&policy,
                       THREAD_AFFINITY_POLICY_COUNT );
}
#endif

static uint64_t now_ticks( void ) {
#if defined( __APPLE__ )
    return mach_absolute_time();
#elif defined( __linux__ ) && defined( __x86_64__ )
    _mm_lfence();
    uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
#else
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return ( uint64_t )ts.tv_sec * 1000000000ull + ( uint64_t )ts.tv_nsec;
#endif
}

static const char *timer_unit( void ) {
#if defined( __APPLE__ )
    return "ticks";
#elif defined( __linux__ ) && defined( __x86_64__ )
    return "cycles";
#else
    return "ns";
#endif
}

static double ticks_to_ms( uint64_t ticks ) {
#if defined( __APPLE__ )
    static mach_timebase_info_data_t tb = { 0, 0 };
    if ( tb.denom == 0 )
        mach_timebase_info( &tb );
    double ns = ( double )ticks * ( double )tb.numer / ( double )tb.denom;
    return ns / 1e6;
#elif defined( __linux__ ) && defined( __x86_64__ )
    return -1.0;
#else
    return ( double )ticks / 1e6;
#endif
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
#if defined( __APPLE__ )
    bind_to_p_core();
#endif

    BenchResult r = { .impl = impl, .available = mandelbrot_impl_available( impl ) };

    if ( !r.available )
        return r;

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

#if defined( __linux__ ) && defined( __x86_64__ )
    printf( "%-8s avg: %12llu cycles | best: %12llu cycles | checksum: %016llx\n",
            mandelbrot_impl_name( impl ), ( unsigned long long )r.avg_ticks,
            ( unsigned long long )r.best_ticks, ( unsigned long long )r.checksum );
#else
    printf( "%-8s avg: %8.3f ms | best: %8.3f ms | checksum: %016llx\n",
            mandelbrot_impl_name( impl ), r.avg_ms, r.best_ms, ( unsigned long long )r.checksum );
#endif

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
                 mandelbrot_impl_name( r->impl ), r->available, ( unsigned long long )r->avg_ticks,
                 ( unsigned long long )r->best_ticks, r->avg_ms, r->best_ms,
                 ( unsigned long long )r->checksum, runs, warmup, WIDTH, HEIGHT, MAX_ITER,
                 timer_unit() );
    }

    fclose( f );
    printf( "CSV saved: %s\n", path );
}

int main( int argc, char **argv ) {
#if defined( __APPLE__ )
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#endif

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
    MandelbrotImpl impls[] = { MANDEL_IMPL_V1, MANDEL_IMPL_V2, MANDEL_IMPL_V3_NEON8,
                               MANDEL_IMPL_V3_NEON4, MANDEL_IMPL_V3_NEON16, MANDEL_IMPL_V3_X86 };
    const int impl_count = ( int )( sizeof( impls ) / sizeof( impls[0] ) );
    BenchResult results[6] = { 0 };

    printf( "Mandelbrot benchmark | %dx%d | MAX_ITER=%d | runs=%d warmup=%d\n", WIDTH, HEIGHT,
            MAX_ITER, runs, warmup );

    uint64_t base_avg_ticks = 0;
    for ( int i = 0; i < impl_count; i++ ) {
        results[i] = run_case( impls[i], buffer, runs, warmup, xmin, xmax, ymin, ymax );

        if ( !results[i].available ) {
            printf( "%-8s skipped (not available in current build)\n",
                    mandelbrot_impl_name( impls[i] ) );
            continue;
        }

        if ( impls[i] == MANDEL_IMPL_V1 ) {
            base_avg_ticks = results[i].avg_ticks;
        } else if ( base_avg_ticks > 0 ) {
            double speedup = ( double )base_avg_ticks / ( double )results[i].avg_ticks;
            printf( "         speedup vs v1: x%.2f\n", speedup );
        }
    }

    if ( csv_path && csv_path[0] != '\0' )
        write_csv( csv_path, results, impl_count, runs, warmup );

    free( buffer );
    return 0;
}
