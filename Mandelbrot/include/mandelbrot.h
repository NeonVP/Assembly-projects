#ifndef MANDELBROT_H
#define MANDELBROT_H

typedef enum MandelbrotImpl {
    MANDEL_IMPL_V1 = 0,
    MANDEL_IMPL_V2,
    MANDEL_IMPL_V3_NEON,
    MANDEL_IMPL_V3_X86
} MandelbrotImpl;

const char *mandelbrot_impl_name( MandelbrotImpl impl );
int mandelbrot_impl_from_string( const char *name, MandelbrotImpl *out_impl );

void mandelbrot_compute( unsigned char *img, int width, int height, int max_iter, float xmin,
                         float xmax, float ymin, float ymax, MandelbrotImpl impl );

#endif
