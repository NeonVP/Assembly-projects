#include "Mandelbrot.h"

#include <immintrin.h>
#include <stdalign.h>
#include <stdint.h>

#define L_MAX 100.0f

static void mandelbrot_v1( unsigned char *img,
                           int width,
                           int height,
                           int max_iter,
                           float xmin,
                           float xmax,
                           float ymin,
                           float ymax ) {
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    for ( int y = 0; y < height; y++ ) {
        float cy = ymin + y * dy;
        int row = y * width;

        for ( int x = 0; x < width; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;

                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + x] = ( unsigned char )( iter >= max_iter ? 255 : iter );
        }
    }
}

enum { VEC_SIZE = 8 };
enum { CMP_LE_OQ = 18 };

typedef union {
    float f[VEC_SIZE];
    uint32_t u[VEC_SIZE];
    int32_t i[VEC_SIZE];
} m256;

typedef union {
    uint32_t u[VEC_SIZE];
    int32_t i[VEC_SIZE];
} m256i;

static inline m256 mm_set_ps( float e7, float e6, float e5, float e4, float e3, float e2, float e1, float e0 ) {
    m256 out = {};
    out.f[0] = e0;
    out.f[1] = e1;
    out.f[2] = e2;
    out.f[3] = e3;
    out.f[4] = e4;
    out.f[5] = e5;
    out.f[6] = e6;
    out.f[7] = e7;
    return out;
}

static inline m256 mm_set1_ps( float a ) {
    m256 out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.f[i] = a;
    return out;
}

static inline m256 mm_add_ps( m256 a, m256 b ) {
    m256 out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.f[i] = a.f[i] + b.f[i];
    return out;
}

static inline m256 mm_sub_ps( m256 a, m256 b ) {
    m256 out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.f[i] = a.f[i] - b.f[i];
    return out;
}

static inline m256 mm_mul_ps( m256 a, m256 b ) {
    m256 out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.f[i] = a.f[i] * b.f[i];
    return out;
}

static inline m256 mm_and_ps( m256 a, m256 b ) {
    m256 out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.u[i] = a.u[i] & b.u[i];
    return out;
}

static inline m256 mm_cmp_ps( m256 a, m256 b, const int imm8 ) {
    m256 out = {};
    if ( imm8 != CMP_LE_OQ )
        return out;

    for ( int i = 0; i < VEC_SIZE; i++ )
        out.u[i] = ( a.f[i] <= b.f[i] ) ? 0xFFFFFFFFu : 0u;
    return out;
}

static inline int mm_movemask_ps( m256 a ) {
    int mask = 0;
    for ( int i = 0; i < VEC_SIZE; i++ )
        mask |= ( int )( ( a.u[i] >> 31 ) & 1u ) << i;
    return mask;
}

static inline m256 mm_blendv_ps( m256 a, m256 b, m256 mask ) {
    m256 out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.u[i] = ( mask.u[i] >> 31 ) ? b.u[i] : a.u[i];
    return out;
}

static inline m256i mm_cvtps_epi32( m256 a ) {
    m256i out = {};
    for ( int i = 0; i < VEC_SIZE; i++ )
        out.i[i] = ( int32_t )a.f[i];
    return out;
}

static inline void mm_storeu_si256( m256i *mem_addr, m256i a ) {
    for ( int i = 0; i < VEC_SIZE; i++ )
        mem_addr->i[i] = a.i[i];
}

static void mandelbrot_v2( unsigned char *img,
                           int width,
                           int height,
                           int max_iter,
                           float xmin,
                           float xmax,
                           float ymin,
                           float ymax ) {
    float step_x = ( xmax - xmin ) / width;
    float step_y = ( ymax - ymin ) / height;
    float cy = ymin;

    m256 radius_max = mm_set1_ps( L_MAX );
    m256 two = mm_set1_ps( 2.0f );
    m256 one_ps = mm_set1_ps( 1.0f );
    m256 vec_step_x = mm_set1_ps( step_x * VEC_SIZE );
    m256 cx_start_delta = mm_set_ps( step_x * 7.0f,
                                     step_x * 6.0f,
                                     step_x * 5.0f,
                                     step_x * 4.0f,
                                     step_x * 3.0f,
                                     step_x * 2.0f,
                                     step_x,
                                     0.0f );

    for ( int py = 0; py < height; py++ ) {
        int row = py * width;
        m256 c_x = mm_add_ps( mm_set1_ps( xmin ), cx_start_delta );
        int px = 0;

        for ( ; px <= width - VEC_SIZE; px += VEC_SIZE ) {
            m256 z_x = { 0 };
            m256 z_y = { 0 };
            m256 iters = { 0 };

            for ( int it = 0; it < max_iter; it++ ) {
                m256 z_x2 = mm_mul_ps( z_x, z_x );
                m256 z_y2 = mm_mul_ps( z_y, z_y );
                m256 z_xy = mm_mul_ps( z_x, z_y );
                m256 radius2 = mm_add_ps( z_x2, z_y2 );
                m256 mask = mm_cmp_ps( radius2, radius_max, CMP_LE_OQ );

                if ( mm_movemask_ps( mask ) == 0 )
                    break;

                m256 next_x = mm_add_ps( mm_sub_ps( z_x2, z_y2 ), c_x );
                m256 next_y = mm_add_ps( mm_mul_ps( two, z_xy ), mm_set1_ps( cy ) );

                z_x = mm_blendv_ps( z_x, next_x, mask );
                z_y = mm_blendv_ps( z_y, next_y, mask );
                iters = mm_add_ps( iters, mm_and_ps( mask, one_ps ) );
            }

            int out_iters[VEC_SIZE] = { 0 };
            mm_storeu_si256( ( m256i * )out_iters, mm_cvtps_epi32( iters ) );

            for ( int i = 0; i < VEC_SIZE; i++ )
                img[row + px + i] = ( unsigned char )( out_iters[i] >= max_iter ? 255 : out_iters[i] );

            c_x = mm_add_ps( c_x, vec_step_x );
        }

        for ( ; px < width; px++ ) {
            float cx = xmin + px * step_x;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + px] = ( unsigned char )( iter >= max_iter ? 255 : iter );
        }

        cy += step_y;
    }
}

static void mandelbrot_v3_x86( unsigned char *img,
                               int width,
                               int height,
                               int max_iter,
                               float xmin,
                               float xmax,
                               float ymin,
                               float ymax ) {
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    __m256 radius = _mm256_set1_ps( L_MAX );
    __m256 two = _mm256_set1_ps( 2.0f );
    __m256 one_ps = _mm256_castsi256_ps( _mm256_set1_epi32( 1 ) );
    __m256 idx = _mm256_set_ps( 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m256 cy = _mm256_set1_ps( cy_scalar );
        int row = y * width;
        int x = 0;

        for ( ; x <= width - 8; x += 8 ) {
            float base_x = xmin + x * dx;
            __m256 cx = _mm256_add_ps( _mm256_set1_ps( base_x ), _mm256_mul_ps( idx, _mm256_set1_ps( dx ) ) );
            __m256 zx = _mm256_setzero_ps();
            __m256 zy = _mm256_setzero_ps();
            __m256 iters = _mm256_setzero_ps();

            for ( int it = 0; it < max_iter; it++ ) {
                __m256 zx2 = _mm256_mul_ps( zx, zx );
                __m256 zy2 = _mm256_mul_ps( zy, zy );
                __m256 radius2 = _mm256_add_ps( zx2, zy2 );
                __m256 mask = _mm256_cmp_ps( radius2, radius, _CMP_LE_OQ );

                if ( _mm256_movemask_ps( mask ) == 0 )
                    break;

                __m256 zxy = _mm256_mul_ps( zx, zy );
                __m256 nx = _mm256_add_ps( _mm256_sub_ps( zx2, zy2 ), cx );
                __m256 ny = _mm256_add_ps( _mm256_mul_ps( two, zxy ), cy );

                zx = _mm256_blendv_ps( zx, nx, mask );
                zy = _mm256_blendv_ps( zy, ny, mask );
                iters = _mm256_add_ps( iters, _mm256_and_ps( mask, one_ps ) );
            }

            alignas( 32 ) float out_iters[8];
            _mm256_store_ps( out_iters, iters );

            for ( int k = 0; k < 8; k++ ) {
                int iter = ( int )out_iters[k];
                img[row + x + k] = ( unsigned char )( iter >= max_iter ? 255 : iter );
            }
        }

        for ( ; x < width; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy_scalar;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + x] = ( unsigned char )( iter >= max_iter ? 255 : iter );
        }
    }
}

static void mandelbrot_v3_x86_avx512( unsigned char *img,
                                      int width,
                                      int height,
                                      int max_iter,
                                      float xmin,
                                      float xmax,
                                      float ymin,
                                      float ymax ) {
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    __m512 radius = _mm512_set1_ps( L_MAX );
    __m512 two = _mm512_set1_ps( 2.0f );
    __m512 one_ps = _mm512_set1_ps( 1.0f );
    __m512 dx_vec = _mm512_set1_ps( dx );
    __m512 idx = _mm512_set_ps( 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f,
                                7.0f,  6.0f,  5.0f,  4.0f,  3.0f,  2.0f,  1.0f, 0.0f );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m512 cy = _mm512_set1_ps( cy_scalar );
        int row = y * width;
        int x = 0;

        for ( ; x <= width - 16; x += 16 ) {
            float base_x = xmin + x * dx;
            __m512 cx = _mm512_add_ps( _mm512_set1_ps( base_x ), _mm512_mul_ps( idx, dx_vec ) );
            __m512 zx = _mm512_setzero_ps();
            __m512 zy = _mm512_setzero_ps();
            __m512 iters = _mm512_setzero_ps();

            for ( int it = 0; it < max_iter; it++ ) {
                __m512 zx2 = _mm512_mul_ps( zx, zx );
                __m512 zy2 = _mm512_mul_ps( zy, zy );
                __m512 radius2 = _mm512_add_ps( zx2, zy2 );
                __mmask16 mask = _mm512_cmp_ps_mask( radius2, radius, _CMP_LE_OQ );

                if ( mask == 0 )
                    break;

                __m512 zxy = _mm512_mul_ps( zx, zy );
                __m512 nx = _mm512_add_ps( _mm512_sub_ps( zx2, zy2 ), cx );
                __m512 ny = _mm512_add_ps( _mm512_mul_ps( two, zxy ), cy );

                zx = _mm512_mask_blend_ps( mask, zx, nx );
                zy = _mm512_mask_blend_ps( mask, zy, ny );
                iters = _mm512_mask_add_ps( iters, mask, iters, one_ps );
            }

            alignas( 64 ) float out_iters[16];
            _mm512_store_ps( out_iters, iters );

            for ( int k = 0; k < 16; k++ ) {
                int iter = ( int )out_iters[k];
                img[row + x + k] = ( unsigned char )( iter >= max_iter ? 255 : iter );
            }
        }

        for ( ; x < width; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy_scalar;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + x] = ( unsigned char )( iter >= max_iter ? 255 : iter );
        }
    }
}

uint64_t mandelbrot_bench_v1_mem( unsigned char *img,
                                  int width,
                                  int height,
                                  int max_iter,
                                  float xmin,
                                  float xmax,
                                  float ymin,
                                  float ymax ) {
    uint64_t checksum = 0;
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    for ( int y = 0; y < height; y++ ) {
        float cy = ymin + y * dy;
        int row = y * width;

        for ( int x = 0; x < width; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;

                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + x] = ( unsigned char )( iter >= max_iter ? 255 : iter );
            checksum += img[row + x];
        }
    }

    return checksum;
}

uint64_t mandelbrot_bench_v1_var( unsigned char *img,
                                  int width,
                                  int height,
                                  int max_iter,
                                  float xmin,
                                  float xmax,
                                  float ymin,
                                  float ymax ) {
    ( void )img;

    uint64_t checksum = 0;
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    for ( int y = 0; y < height; y++ ) {
        float cy = ymin + y * dy;

        for ( int x = 0; x < width; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;

                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            checksum += ( unsigned char )( iter >= max_iter ? 255 : iter );
        }
    }

    return checksum;
}

uint64_t mandelbrot_bench_v2_var( unsigned char *img,
                                  int width,
                                  int height,
                                  int max_iter,
                                  float xmin,
                                  float xmax,
                                  float ymin,
                                  float ymax ) {
    ( void )img;

    uint64_t checksum = 0;
    float step_x = ( xmax - xmin ) / width;
    float step_y = ( ymax - ymin ) / height;
    float cy = ymin;

    m256 radius_max = mm_set1_ps( L_MAX );
    m256 two = mm_set1_ps( 2.0f );
    m256 one_ps = mm_set1_ps( 1.0f );
    m256 vec_step_x = mm_set1_ps( step_x * VEC_SIZE );
    m256 cx_start_delta = mm_set_ps( step_x * 7.0f,
                                     step_x * 6.0f,
                                     step_x * 5.0f,
                                     step_x * 4.0f,
                                     step_x * 3.0f,
                                     step_x * 2.0f,
                                     step_x,
                                     0.0f );

    for ( int py = 0; py < height; py++ ) {
        m256 c_x = mm_add_ps( mm_set1_ps( xmin ), cx_start_delta );
        int px = 0;

        for ( ; px <= width - VEC_SIZE; px += VEC_SIZE ) {
            m256 z_x = { 0 };
            m256 z_y = { 0 };
            m256 iters = { 0 };

            for ( int it = 0; it < max_iter; it++ ) {
                m256 z_x2 = mm_mul_ps( z_x, z_x );
                m256 z_y2 = mm_mul_ps( z_y, z_y );
                m256 z_xy = mm_mul_ps( z_x, z_y );
                m256 radius2 = mm_add_ps( z_x2, z_y2 );
                m256 mask = mm_cmp_ps( radius2, radius_max, CMP_LE_OQ );

                if ( mm_movemask_ps( mask ) == 0 )
                    break;

                m256 next_x = mm_add_ps( mm_sub_ps( z_x2, z_y2 ), c_x );
                m256 next_y = mm_add_ps( mm_mul_ps( two, z_xy ), mm_set1_ps( cy ) );

                z_x = mm_blendv_ps( z_x, next_x, mask );
                z_y = mm_blendv_ps( z_y, next_y, mask );
                iters = mm_add_ps( iters, mm_and_ps( mask, one_ps ) );
            }

            int out_iters[VEC_SIZE] = { 0 };
            mm_storeu_si256( ( m256i * )out_iters, mm_cvtps_epi32( iters ) );

            for ( int i = 0; i < VEC_SIZE; i++ ) {
                int iter = out_iters[i] >= max_iter ? 255 : out_iters[i];
                checksum += ( unsigned char )iter;
            }

            c_x = mm_add_ps( c_x, vec_step_x );
        }

        for ( ; px < width; px++ ) {
            float cx = xmin + px * step_x;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            checksum += ( unsigned char )( iter >= max_iter ? 255 : iter );
        }

        cy += step_y;
    }

    return checksum;
}

uint64_t mandelbrot_bench_v3_x86_var( unsigned char *img,
                                      int width,
                                      int height,
                                      int max_iter,
                                      float xmin,
                                      float xmax,
                                      float ymin,
                                      float ymax ) {
    ( void )img;

    uint64_t checksum = 0;
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    __m256 radius = _mm256_set1_ps( L_MAX );
    __m256 two = _mm256_set1_ps( 2.0f );
    __m256 one_ps = _mm256_set1_ps( 1.0f );
    __m256 idx = _mm256_set_ps( 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f );
    __m256 dx_vec = _mm256_set1_ps( dx );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m256 cy = _mm256_set1_ps( cy_scalar );
        int x = 0;

        for ( ; x <= width - 8; x += 8 ) {
            float base_x = xmin + x * dx;
            __m256 cx = _mm256_add_ps( _mm256_set1_ps( base_x ), _mm256_mul_ps( idx, dx_vec ) );
            __m256 zx = _mm256_setzero_ps();
            __m256 zy = _mm256_setzero_ps();
            __m256 iters = _mm256_setzero_ps();

            for ( int it = 0; it < max_iter; it++ ) {
                __m256 zx2 = _mm256_mul_ps( zx, zx );
                __m256 zy2 = _mm256_mul_ps( zy, zy );
                __m256 radius2 = _mm256_add_ps( zx2, zy2 );
                __m256 mask = _mm256_cmp_ps( radius2, radius, _CMP_LE_OQ );

                if ( _mm256_movemask_ps( mask ) == 0 )
                    break;

                __m256 zxy = _mm256_mul_ps( zx, zy );
                __m256 nx = _mm256_add_ps( _mm256_sub_ps( zx2, zy2 ), cx );
                __m256 ny = _mm256_add_ps( _mm256_mul_ps( two, zxy ), cy );

                zx = _mm256_blendv_ps( zx, nx, mask );
                zy = _mm256_blendv_ps( zy, ny, mask );
                iters = _mm256_add_ps( iters, _mm256_and_ps( mask, one_ps ) );
            }

            alignas( 32 ) float out_iters[8];
            _mm256_store_ps( out_iters, iters );

            for ( int lane = 0; lane < 8; lane++ ) {
                int iter = ( int )out_iters[lane];
                checksum += ( unsigned char )( iter >= max_iter ? 255 : iter );
            }
        }

        for ( ; x < width; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy_scalar;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            checksum += ( unsigned char )( iter >= max_iter ? 255 : iter );
        }
    }

    return checksum;
}

uint64_t mandelbrot_bench_v3_x86_avx512_var( unsigned char *img,
                                             int width,
                                             int height,
                                             int max_iter,
                                             float xmin,
                                             float xmax,
                                             float ymin,
                                             float ymax ) {
    ( void )img;
    uint64_t checksum = 0;
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    __m512 radius2_max = _mm512_set1_ps( L_MAX );
    __m512i v_one = _mm512_set1_epi32( 1 ); 
    __m512 dx_vec = _mm512_set1_ps( dx );
    
    __m512 idx = _mm512_set_ps( 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f,
                                7.0f,  6.0f,  5.0f,  4.0f,  3.0f,  2.0f,  1.0f, 0.0f );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m512 cy = _mm512_set1_ps( cy_scalar );
        int x = 0;

        for ( ; x <= width - 16; x += 16 ) {
            float base_x = xmin + x * dx;
            __m512 cx = _mm512_add_ps( _mm512_set1_ps( base_x ), _mm512_mul_ps( idx, dx_vec ) );

            __m512 zx = _mm512_setzero_ps();
            __m512 zy = _mm512_setzero_ps();
            __m512i iters = _mm512_setzero_si512();

            for ( int it = 0; it < max_iter; it++ ) {
                __m512 zx2 = _mm512_mul_ps( zx, zx );
                __m512 zy2 = _mm512_mul_ps( zy, zy );
                __m512 r2 = _mm512_add_ps( zx2, zy2 );
                
                // Проверка выхода за радиус
                __mmask16 mask = _mm512_cmp_ps_mask( r2, radius2_max, _CMP_LE_OQ );

                if ( mask == 0 ) break;

                __m512 zxy = _mm512_mul_ps( zx, zy );
                
                // Стандартный расчет математики (как в твоей C++ версии)
                __m512 nx = _mm512_add_ps( _mm512_sub_ps( zx2, zy2 ), cx );
                // 2 * x * y заменяем на (xy + xy), чтобы не тратить такты на _mm512_mul_ps
                __m512 ny = _mm512_add_ps( _mm512_add_ps( zxy, zxy ), cy );

                // Обновляем значения только по маске
                zx = _mm512_mask_blend_ps( mask, zx, nx );
                zy = _mm512_mask_blend_ps( mask, zy, ny );
                iters = _mm512_mask_add_epi32( iters, mask, iters, v_one );
            }

            alignas( 64 ) uint32_t out[16];
            _mm512_store_si512( (__m512i*)out, iters );

            for ( int lane = 0; lane < 16; lane++ ) {
                checksum += ( unsigned char )( out[lane] >= (uint32_t)max_iter ? 255 : out[lane] );
            }
        }

        // Хвостовой цикл для оставшихся пикселей (если ширина не кратна 16)
        for ( ; x < width; x++ ) {
            float cx_scalar = xmin + x * dx;
            float zx_scalar = 0.0f;
            float zy_scalar = 0.0f;
            int iter = 0;

            while ( ( zx_scalar * zx_scalar + zy_scalar * zy_scalar <= L_MAX ) && ( iter < max_iter ) ) {
                float zx2 = zx_scalar * zx_scalar;
                float zy2 = zy_scalar * zy_scalar;
                float zxy = zx_scalar * zy_scalar;
                zy_scalar = 2.0f * zxy + cy_scalar;
                zx_scalar = zx2 - zy2 + cx_scalar;
                iter++;
            }
            checksum += ( unsigned char )( iter >= max_iter ? 255 : iter );
        }
    }

    return checksum;
}

const char *mandelbrot_impl_name( MandelbrotImpl impl ) {
    switch ( impl ) {
    case MANDEL_IMPL_V1:
        return "v1";
    case MANDEL_IMPL_V2:
        return "v2";
    case MANDEL_IMPL_V3_NEON8:
        return "v3neon8";
    case MANDEL_IMPL_V3_NEON4:
        return "v3neon4";
    case MANDEL_IMPL_V3_NEON16:
        return "v3neon16";
    case MANDEL_IMPL_V3_X86:
        return "v3x86";
    case MANDEL_IMPL_V3_X86_AVX512:
        return "v3x86_avx512";
    default:
        return "unknown";
    }
}

void mandelbrot_compute( unsigned char *img, int width, int height, int max_iter,
                         float xmin, float xmax,
                         float ymin, float ymax,
                         MandelbrotImpl impl ) {
    switch ( impl ) {
    case MANDEL_IMPL_V1:
        mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
        return;
    case MANDEL_IMPL_V2:
        mandelbrot_v2( img, width, height, max_iter, xmin, xmax, ymin, ymax );
        return;
    case MANDEL_IMPL_V3_X86:
        mandelbrot_v3_x86( img, width, height, max_iter, xmin, xmax, ymin, ymax );
        return;
    case MANDEL_IMPL_V3_X86_AVX512:
        mandelbrot_v3_x86_avx512( img, width, height, max_iter, xmin, xmax, ymin, ymax );
        return;
    default:
        mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
        return;
    }
}

uint64_t mandelbrot_bench_compute( unsigned char *img, int width, int height,
                                   int max_iter,
                                   float xmin, float xmax,
                                   float ymin, float ymax,
                                   MandelbrotImpl impl ) {
    switch ( impl ) {
    case MANDEL_IMPL_V1:
        return mandelbrot_bench_v1_mem( img, width, height, max_iter, xmin, xmax, ymin, ymax );
    case MANDEL_IMPL_V2:
        return mandelbrot_bench_v2_var( img, width, height, max_iter, xmin, xmax, ymin, ymax );
    case MANDEL_IMPL_V3_X86:
        return mandelbrot_bench_v3_x86_var( img, width, height, max_iter, xmin, xmax, ymin, ymax );
    case MANDEL_IMPL_V3_X86_AVX512:
        return mandelbrot_bench_v3_x86_avx512_var( img, width, height, max_iter, xmin, xmax, ymin, ymax );
    default:
        mandelbrot_compute( img, width, height, max_iter, xmin, xmax, ymin, ymax, impl );
        return 0;
    }
}
