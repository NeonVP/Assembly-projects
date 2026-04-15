#include "Mandelbrot.h"

#include <stdint.h>
#include <string.h>

#define L_MAX 100.0f

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#if defined(__AVX__)
#include <immintrin.h>
#endif

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
enum { _CMP_LE_OQ = 18 };

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
    if ( imm8 != _CMP_LE_OQ )
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
                m256 mask = mm_cmp_ps( radius2, radius_max, _CMP_LE_OQ );

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

static void mandelbrot_v3_neon8( unsigned char *img,
                                int width,
                                int height,
                                int max_iter,
                                float xmin,
                                float xmax,
                                float ymin,
                                float ymax ) {
#if defined(__aarch64__) || defined(__ARM_NEON)
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    // Просто копируем одно значение сразу во все ячейки вектора.
    float32x4_t radius = vdupq_n_f32( L_MAX );
    float32x4_t two = vdupq_n_f32( 2.0f );
    uint32x4_t one = vdupq_n_u32( 1 );
    uint32x4_t max_u8 = vdupq_n_u32( 255 );

    float32x4_t idx0 = { 0.0f, 1.0f, 2.0f, 3.0f };
    float32x4_t idx1 = { 4.0f, 5.0f, 6.0f, 7.0f };
    float32x4_t dx_vec = vdupq_n_f32( dx );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * width;

        int x = 0;
        for ( ; x <= width - 8; x += 8 ) {
            float base_x = xmin + x * dx;

            // Тут считаем cx сразу для 8 соседних пикселей.
            float32x4_t cx0 = vmlaq_f32( vdupq_n_f32( base_x ), idx0, dx_vec );
            float32x4_t cx1 = vmlaq_f32( vdupq_n_f32( base_x ), idx1, dx_vec );

            float32x4_t zx0 = vdupq_n_f32( 0.0f );
            float32x4_t zy0 = vdupq_n_f32( 0.0f );
            float32x4_t zx1 = vdupq_n_f32( 0.0f );
            float32x4_t zy1 = vdupq_n_f32( 0.0f );

            uint32x4_t it0 = vdupq_n_u32( 0 );
            uint32x4_t it1 = vdupq_n_u32( 0 );

            for ( int it = 0; it < max_iter; it++ ) {
                float32x4_t zx2_0 = vmulq_f32( zx0, zx0 );
                float32x4_t zy2_0 = vmulq_f32( zy0, zy0 );
                float32x4_t zx2_1 = vmulq_f32( zx1, zx1 );
                float32x4_t zy2_1 = vmulq_f32( zy1, zy1 );

                // Проверяем, какие точки еще "живые".
                uint32x4_t m0 = vcleq_f32( vaddq_f32( zx2_0, zy2_0 ), radius );
                uint32x4_t m1 = vcleq_f32( vaddq_f32( zx2_1, zy2_1 ), radius );

                if ( ( it & 3 ) == 0 ) {
                    uint32x4_t m = vorrq_u32( m0, m1 );
                    // Если все улетели, дальше считать уже не нужно.
                    if ( vaddvq_u32( m ) == 0 )
                        break;
                }

                float32x4_t zxy0 = vmulq_f32( zx0, zy0 );
                float32x4_t zxy1 = vmulq_f32( zx1, zy1 );

                float32x4_t nx0 = vaddq_f32( vsubq_f32( zx2_0, zy2_0 ), cx0 );
                float32x4_t nx1 = vaddq_f32( vsubq_f32( zx2_1, zy2_1 ), cx1 );

                float32x4_t ny0 = vfmaq_f32( cy, zxy0, two );
                float32x4_t ny1 = vfmaq_f32( cy, zxy1, two );

                // Обновляем только те точки, которые еще считаются.
                zx0 = vbslq_f32( m0, nx0, zx0 );
                zy0 = vbslq_f32( m0, ny0, zy0 );
                zx1 = vbslq_f32( m1, nx1, zx1 );
                zy1 = vbslq_f32( m1, ny1, zy1 );

                // +1 к итерациям только у активных точек.
                it0 = vaddq_u32( it0, vandq_u32( m0, one ) );
                it1 = vaddq_u32( it1, vandq_u32( m1, one ) );
            }

            it0 = vminq_u32( it0, max_u8 );
            it1 = vminq_u32( it1, max_u8 );

            // Ужимаем счетчики до байтов, чтобы быстро записать в картинку.
            uint16x4_t it0_16 = vmovn_u32( it0 );
            uint16x4_t it1_16 = vmovn_u32( it1 );
            uint16x8_t it16 = vcombine_u16( it0_16, it1_16 );
            uint8x8_t it8 = vmovn_u16( it16 );
            vst1_u8( &img[row + x], it8 );
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
#else
    mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
#endif
}

static void mandelbrot_v3_neon4( unsigned char *img,
                                 int width,
                                 int height,
                                 int max_iter,
                                 float xmin,
                                 float xmax,
                                 float ymin,
                                 float ymax ) {
#if defined(__aarch64__) || defined(__ARM_NEON)
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    // Копируем одно значение во все 4 ячейки вектора
    float32x4_t radius = vdupq_n_f32( L_MAX );
    float32x4_t two = vdupq_n_f32( 2.0f );
    uint32x4_t one = vdupq_n_u32( 1 );

    float32x4_t idx = { 0.0f, 1.0f, 2.0f, 3.0f };
    float32x4_t dx_vec = vdupq_n_f32( dx );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * width;

        int x = 0;
        for ( ; x <= width - 4; x += 4 ) {
            float base_x = xmin + x * dx;
            // Считаем сразу 4 соседних cx
            float32x4_t cx = vmlaq_f32( vdupq_n_f32( base_x ), idx, dx_vec );

            float32x4_t zx = vdupq_n_f32( 0.0f );
            float32x4_t zy = vdupq_n_f32( 0.0f );
            uint32x4_t it = vdupq_n_u32( 0 );

            for ( int k = 0; k < max_iter; k++ ) {
                float32x4_t zx2 = vmulq_f32( zx, zx );
                float32x4_t zy2 = vmulq_f32( zy, zy );

                // Какие точки еще считаются
                uint32x4_t mask = vcleq_f32( vaddq_f32( zx2, zy2 ), radius );
                if ( vaddvq_u32( mask ) == 0 )
                    break;

                float32x4_t zxy = vmulq_f32( zx, zy );
                float32x4_t nx = vaddq_f32( vsubq_f32( zx2, zy2 ), cx );
                float32x4_t ny = vfmaq_f32( cy, zxy, two );

                // Обновляем только активные точки
                zx = vbslq_f32( mask, nx, zx );
                zy = vbslq_f32( mask, ny, zy );
                it = vaddq_u32( it, vandq_u32( mask, one ) );
            }

            uint32_t out_iters[4];
            vst1q_u32( out_iters, it );
            for ( int i = 0; i < 4; i++ )
                img[row + x + i] = ( unsigned char )( out_iters[i] >= ( uint32_t )max_iter ? 255 : out_iters[i] );
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
#else
    mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
#endif
}

static void mandelbrot_v3_neon16( unsigned char *img,
                                  int width,
                                  int height,
                                  int max_iter,
                                  float xmin,
                                  float xmax,
                                  float ymin,
                                  float ymax ) {
#if defined(__aarch64__) || defined(__ARM_NEON)
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    // Просто копируем одно значение сразу во все ячейки вектора
    float32x4_t radius = vdupq_n_f32( L_MAX );
    float32x4_t two = vdupq_n_f32( 2.0f );
    uint32x4_t one = vdupq_n_u32( 1 );
    uint32x4_t max_u8 = vdupq_n_u32( 255 );

    float32x4_t idx0 = { 0.0f, 1.0f, 2.0f, 3.0f };
    float32x4_t idx1 = { 4.0f, 5.0f, 6.0f, 7.0f };
    float32x4_t idx2 = { 8.0f, 9.0f, 10.0f, 11.0f };
    float32x4_t idx3 = { 12.0f, 13.0f, 14.0f, 15.0f };
    float32x4_t dx_vec = vdupq_n_f32( dx );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * width;

        int x = 0;
        for ( ; x <= width - 16; x += 16 ) {
            float base_x = xmin + x * dx;

            // Тут считаем cx сразу для 16 соседних пикселей
            float32x4_t cx0 = vmlaq_f32( vdupq_n_f32( base_x ), idx0, dx_vec );
            float32x4_t cx1 = vmlaq_f32( vdupq_n_f32( base_x ), idx1, dx_vec );
            float32x4_t cx2 = vmlaq_f32( vdupq_n_f32( base_x ), idx2, dx_vec );
            float32x4_t cx3 = vmlaq_f32( vdupq_n_f32( base_x ), idx3, dx_vec );

            float32x4_t zx0 = vdupq_n_f32( 0.0f );
            float32x4_t zy0 = vdupq_n_f32( 0.0f );
            float32x4_t zx1 = vdupq_n_f32( 0.0f );
            float32x4_t zy1 = vdupq_n_f32( 0.0f );
            float32x4_t zx2 = vdupq_n_f32( 0.0f );
            float32x4_t zy2 = vdupq_n_f32( 0.0f );
            float32x4_t zx3 = vdupq_n_f32( 0.0f );
            float32x4_t zy3 = vdupq_n_f32( 0.0f );

            uint32x4_t it0 = vdupq_n_u32( 0 );
            uint32x4_t it1 = vdupq_n_u32( 0 );
            uint32x4_t it2 = vdupq_n_u32( 0 );
            uint32x4_t it3 = vdupq_n_u32( 0 );

            for ( int it = 0; it < max_iter; it++ ) {
                float32x4_t zx2_0 = vmulq_f32( zx0, zx0 );
                float32x4_t zy2_0 = vmulq_f32( zy0, zy0 );
                float32x4_t zx2_1 = vmulq_f32( zx1, zx1 );
                float32x4_t zy2_1 = vmulq_f32( zy1, zy1 );
                float32x4_t zx2_2 = vmulq_f32( zx2, zx2 );
                float32x4_t zy2_2 = vmulq_f32( zy2, zy2 );
                float32x4_t zx2_3 = vmulq_f32( zx3, zx3 );
                float32x4_t zy2_3 = vmulq_f32( zy3, zy3 );

                // Проверяем, какие точки еще "живые"
                uint32x4_t m0 = vcleq_f32( vaddq_f32( zx2_0, zy2_0 ), radius );
                uint32x4_t m1 = vcleq_f32( vaddq_f32( zx2_1, zy2_1 ), radius );
                uint32x4_t m2 = vcleq_f32( vaddq_f32( zx2_2, zy2_2 ), radius );
                uint32x4_t m3 = vcleq_f32( vaddq_f32( zx2_3, zy2_3 ), radius );

                if ( ( it & 3 ) == 0 ) {
                    uint32x4_t m = vorrq_u32( vorrq_u32( m0, m1 ), vorrq_u32( m2, m3 ) );
                    // Если все улетели, дальше считать уже не нужно
                    if ( vaddvq_u32( m ) == 0 )
                        break;
                }

                float32x4_t zxy0 = vmulq_f32( zx0, zy0 );
                float32x4_t zxy1 = vmulq_f32( zx1, zy1 );
                float32x4_t zxy2 = vmulq_f32( zx2, zy2 );
                float32x4_t zxy3 = vmulq_f32( zx3, zy3 );

                float32x4_t nx0 = vaddq_f32( vsubq_f32( zx2_0, zy2_0 ), cx0 );
                float32x4_t nx1 = vaddq_f32( vsubq_f32( zx2_1, zy2_1 ), cx1 );
                float32x4_t nx2 = vaddq_f32( vsubq_f32( zx2_2, zy2_2 ), cx2 );
                float32x4_t nx3 = vaddq_f32( vsubq_f32( zx2_3, zy2_3 ), cx3 );

                float32x4_t ny0 = vfmaq_f32( cy, zxy0, two );
                float32x4_t ny1 = vfmaq_f32( cy, zxy1, two );
                float32x4_t ny2 = vfmaq_f32( cy, zxy2, two );
                float32x4_t ny3 = vfmaq_f32( cy, zxy3, two );

                // Обновляем только те точки, которые еще считаются
                zx0 = vbslq_f32( m0, nx0, zx0 );
                zy0 = vbslq_f32( m0, ny0, zy0 );
                zx1 = vbslq_f32( m1, nx1, zx1 );
                zy1 = vbslq_f32( m1, ny1, zy1 );
                zx2 = vbslq_f32( m2, nx2, zx2 );
                zy2 = vbslq_f32( m2, ny2, zy2 );
                zx3 = vbslq_f32( m3, nx3, zx3 );
                zy3 = vbslq_f32( m3, ny3, zy3 );

                // +1 к итерациям только у активных точек
                it0 = vaddq_u32( it0, vandq_u32( m0, one ) );
                it1 = vaddq_u32( it1, vandq_u32( m1, one ) );
                it2 = vaddq_u32( it2, vandq_u32( m2, one ) );
                it3 = vaddq_u32( it3, vandq_u32( m3, one ) );
            }

            it0 = vminq_u32( it0, max_u8 );
            it1 = vminq_u32( it1, max_u8 );
            it2 = vminq_u32( it2, max_u8 );
            it3 = vminq_u32( it3, max_u8 );

            uint32_t out0[4];
            uint32_t out1[4];
            uint32_t out2[4];
            uint32_t out3[4];
            vst1q_u32( out0, it0 );
            vst1q_u32( out1, it1 );
            vst1q_u32( out2, it2 );
            vst1q_u32( out3, it3 );

            for ( int i = 0; i < 4; i++ ) {
                img[row + x + i] = ( unsigned char )out0[i];
                img[row + x + 4 + i] = ( unsigned char )out1[i];
                img[row + x + 8 + i] = ( unsigned char )out2[i];
                img[row + x + 12 + i] = ( unsigned char )out3[i];
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
#else
    mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
#endif
}

static void mandelbrot_v3_x86( unsigned char *img,
                               int width,
                               int height,
                               int max_iter,
                               float xmin,
                               float xmax,
                               float ymin,
                               float ymax ) {
#if defined(__AVX__)
    float dx = ( xmax - xmin ) / width;
    float dy = ( ymax - ymin ) / height;

    // Просто копируем одно значение сразу во все 8 ячеек вектора.
    __m256 radius = _mm256_set1_ps( L_MAX );
    __m256 two = _mm256_set1_ps( 2.0f );
    __m256 one_ps = _mm256_castsi256_ps( _mm256_set1_epi32( 1 ) );
    // Номера позиций внутри вектора: 0..7.
    __m256 idx = _mm256_set_ps( 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f );

    for ( int y = 0; y < height; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m256 cy = _mm256_set1_ps( cy_scalar );
        int row = y * width;
        int x = 0;

        for ( ; x <= width - 8; x += 8 ) {
            float base_x = xmin + x * dx;
            // Тут сразу считаем 8 соседних cx за один раз.
            __m256 cx = _mm256_add_ps( _mm256_set1_ps( base_x ), _mm256_mul_ps( idx, _mm256_set1_ps( dx ) ) );
            __m256 zx = _mm256_setzero_ps();
            __m256 zy = _mm256_setzero_ps();
            __m256 iters = _mm256_setzero_ps();

            for ( int it = 0; it < max_iter; it++ ) {
                __m256 zx2 = _mm256_mul_ps( zx, zx );
                __m256 zy2 = _mm256_mul_ps( zy, zy );
                __m256 radius2 = _mm256_add_ps( zx2, zy2 );
                // Проверяем, какие из 8 точек еще "живые" (не улетели за радиус).
                __m256 mask = _mm256_cmp_ps( radius2, radius, _CMP_LE_OQ );

                // Если все 8 уже улетели, дальше крутить цикл бессмысленно.
                if ( _mm256_movemask_ps( mask ) == 0 )
                    break;

                __m256 zxy = _mm256_mul_ps( zx, zy );
                __m256 nx = _mm256_add_ps( _mm256_sub_ps( zx2, zy2 ), cx );
                __m256 ny = _mm256_add_ps( _mm256_mul_ps( two, zxy ), cy );

                // Обновляем только те точки, которые еще считаются.
                zx = _mm256_blendv_ps( zx, nx, mask );
                zy = _mm256_blendv_ps( zy, ny, mask );
                // +1 к итерациям только у активных точек.
                iters = _mm256_add_ps( iters, _mm256_and_ps( mask, one_ps ) );
            }

            alignas( 32 ) float out_iters[8];
            // Выгружаем 8 счетчиков из вектора в обычный массив.
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
#else
    mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
#endif
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
    default:
        return "unknown";
    }
}

int mandelbrot_impl_from_string( const char *name, MandelbrotImpl *out_impl ) {
    if ( !name || !out_impl )
        return 0;

    if ( strcmp( name, "v1" ) == 0 || strcmp( name, "naive" ) == 0 ) {
        *out_impl = MANDEL_IMPL_V1;
        return 1;
    }
    if ( strcmp( name, "v2" ) == 0 || strcmp( name, "arrayed" ) == 0 ) {
        *out_impl = MANDEL_IMPL_V2;
        return 1;
    }
    if ( strcmp( name, "v3neon8" ) == 0 || strcmp( name, "v3_neon8" ) == 0 ||
         strcmp( name, "v3neon" ) == 0 || strcmp( name, "v3_neon" ) == 0 ||
         strcmp( name, "neon" ) == 0 ) {
        *out_impl = MANDEL_IMPL_V3_NEON8;
        return 1;
    }
    if ( strcmp( name, "v3neon4" ) == 0 || strcmp( name, "v3_neon4" ) == 0 || strcmp( name, "neon4" ) == 0 ) {
        *out_impl = MANDEL_IMPL_V3_NEON4;
        return 1;
    }
    if ( strcmp( name, "v3neon16" ) == 0 || strcmp( name, "v3_neon16" ) == 0 || strcmp( name, "neon16" ) == 0 ) {
        *out_impl = MANDEL_IMPL_V3_NEON16;
        return 1;
    }
    if ( strcmp( name, "v3x86" ) == 0 || strcmp( name, "v3_x86" ) == 0 || strcmp( name, "x86" ) == 0 ) {
        *out_impl = MANDEL_IMPL_V3_X86;
        return 1;
    }

    return 0;
}

int mandelbrot_impl_available( MandelbrotImpl impl ) {
    switch ( impl ) {
    case MANDEL_IMPL_V1:
    case MANDEL_IMPL_V2:
        return 1;
    case MANDEL_IMPL_V3_NEON8:
#if defined(__aarch64__) || defined(__ARM_NEON)
        return 1;
#else
        return 0;
#endif
    case MANDEL_IMPL_V3_NEON4:
#if defined(__aarch64__) || defined(__ARM_NEON)
        return 1;
#else
        return 0;
#endif
    case MANDEL_IMPL_V3_NEON16:
#if defined(__aarch64__) || defined(__ARM_NEON)
        return 1;
#else
        return 0;
#endif
    case MANDEL_IMPL_V3_X86:
#if defined(__AVX__)
        return 1;
#else
        return 0;
#endif
    case MANDEL_IMPL_V3_X86_AVX512:
#if defined(__AVX512F__)
        return 1;
#else
        return 0;
#endif
    default:
        return 0;
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
        case MANDEL_IMPL_V3_NEON8:
            mandelbrot_v3_neon8( img, width, height, max_iter, xmin, xmax, ymin, ymax );
            return;
        case MANDEL_IMPL_V3_NEON4:
            mandelbrot_v3_neon4( img, width, height, max_iter, xmin, xmax, ymin, ymax );
            return;
        case MANDEL_IMPL_V3_NEON16:
            mandelbrot_v3_neon16( img, width, height, max_iter, xmin, xmax, ymin, ymax );
            return;
        case MANDEL_IMPL_V3_X86:
            mandelbrot_v3_x86( img, width, height, max_iter, xmin, xmax, ymin, ymax );
            return;
        default:
            mandelbrot_v1( img, width, height, max_iter, xmin, xmax, ymin, ymax );
            return;
    }
}
