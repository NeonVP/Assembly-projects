#ifndef MANDELBROT_H
#define MANDELBROT_H

#include <stdint.h>

typedef enum MandelbrotImpl {
    MANDEL_IMPL_V1 = 0,
    MANDEL_IMPL_V2,
    MANDEL_IMPL_V3_NEON8,
    MANDEL_IMPL_V3_NEON4,
    MANDEL_IMPL_V3_NEON16,
    MANDEL_IMPL_V3_X86,
    MANDEL_IMPL_V3_X86_AVX512
} MandelbrotImpl;

const char *mandelbrot_impl_name( MandelbrotImpl impl );

void mandelbrot_compute( unsigned char *img, int width, int height, 
                         int max_iter, 
                         float xmin, float xmax, 
                         float ymin, float ymax, 
                         MandelbrotImpl impl );

uint64_t mandelbrot_bench_compute( unsigned char *img, int width, int height,
                                   int max_iter,
                                   float xmin, float xmax,
                                   float ymin, float ymax,
                                   MandelbrotImpl impl );

uint64_t mandelbrot_bench_v1_mem( unsigned char *img, int width, int height,
                                  int max_iter,
                                  float xmin, float xmax,
                                  float ymin, float ymax );
uint64_t mandelbrot_bench_v1_var( unsigned char *img, int width, int height,
                                  int max_iter,
                                  float xmin, float xmax,
                                  float ymin, float ymax );
uint64_t mandelbrot_bench_v2_var( unsigned char *img, int width, int height,
                                  int max_iter,
                                  float xmin, float xmax,
                                  float ymin, float ymax );
uint64_t mandelbrot_bench_v3_x86_var( unsigned char *img, int width, int height,
                                      int max_iter,
                                      float xmin, float xmax,
                                      float ymin, float ymax );
uint64_t mandelbrot_bench_v3_x86_avx512_var( unsigned char *img, int width, int height,
                                             int max_iter,
                                             float xmin, float xmax,
                                             float ymin, float ymax );

#endif
