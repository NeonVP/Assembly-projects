#include "raylib.h"
#include <stdint.h>
#include <stdlib.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

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

static inline m256 mm_set_ps( float e7, float e6, float e5, float e4, 
                              float e3, float e2, float e1, float e0 ) {
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

static void compute_v2( unsigned char *img, float xmin, float xmax, float ymin, float ymax ) {
    float step_x = ( xmax - xmin ) / WIDTH;
    float step_y = ( ymax - ymin ) / HEIGHT;
    float cy = ymin;

    m256 radius_max = mm_set1_ps( L_MAX );
    m256 two = mm_set1_ps( 2.0f );
    m256 one_ps = mm_set1_ps( 1.0f );
    m256 vec_step_x = mm_set1_ps( step_x * VEC_SIZE );
    m256 cx_start_delta = mm_set_ps( step_x * 7.0f, step_x * 6.0f, step_x * 5.0f, step_x * 4.0f,
                                     step_x * 3.0f, step_x * 2.0f, step_x, 0.0f );

    for ( int py = 0; py < HEIGHT; py++ ) {
        int row = py * WIDTH;
        m256 c_x = mm_add_ps( mm_set1_ps( xmin ), cx_start_delta );

        int px = 0;
        for ( ; px <= WIDTH - VEC_SIZE; px += VEC_SIZE ) {
            m256 z_x = { 0 };
            m256 z_y = { 0 };
            m256 iters = { 0 };

            for ( int it = 0; it < MAX_ITER; it++ ) {
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
                img[row + px + i] =
                    ( unsigned char )( out_iters[i] >= MAX_ITER ? 255 : out_iters[i] );

            c_x = mm_add_ps( c_x, vec_step_x );
        }

        for ( ; px < WIDTH; px++ ) {
            float cx = xmin + px * step_x;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;

            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < MAX_ITER ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }

            img[row + px] = ( unsigned char )( iter >= MAX_ITER ? 255 : iter );
        }

        cy += step_y;
    }
}

static Color palette( unsigned char iter ) {
    if ( iter == 255 )
        return BLACK;
    return ( Color ){ iter * 9, iter * 7, iter * 5, 255 };
}

int main( void ) {
    InitWindow( WIDTH, HEIGHT, "Mandelbrot v2" );
    SetTargetFPS( 300 );

    unsigned char *buffer = malloc( WIDTH * HEIGHT );
    Image img = { .data = malloc( WIDTH * HEIGHT * sizeof( Color ) ),
                  .width = WIDTH,
                  .height = HEIGHT,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                  .mipmaps = 1 };

    Texture2D texture = LoadTextureFromImage( img );

    float centerX = -0.5f;
    float centerY = 0.0f;
    float scale = 3.0f;

    while ( !WindowShouldClose() ) {
        float moveSpeed = scale * 0.08f;

        if ( IsKeyDown( KEY_W ) || IsKeyDown( KEY_UP ) )
            centerY -= moveSpeed;
        if ( IsKeyDown( KEY_S ) || IsKeyDown( KEY_DOWN ) )
            centerY += moveSpeed;
        if ( IsKeyDown( KEY_A ) || IsKeyDown( KEY_LEFT ) )
            centerX -= moveSpeed;
        if ( IsKeyDown( KEY_D ) || IsKeyDown( KEY_RIGHT ) )
            centerX += moveSpeed;

        float wheel = GetMouseWheelMove();
        if ( wheel != 0.0f )
            scale *= ( wheel > 0.0f ) ? 0.9f : 1.1f;

        float xmin = centerX - scale;
        float xmax = centerX + scale;
        float ymin = centerY - scale * HEIGHT / WIDTH;
        float ymax = centerY + scale * HEIGHT / WIDTH;

        compute_v2( buffer, xmin, xmax, ymin, ymax );

        Color *pixels = ( Color * )img.data;
        for ( int i = 0; i < WIDTH * HEIGHT; i++ )
            pixels[i] = palette( buffer[i] );

        UpdateTexture( texture, pixels );

        BeginDrawing();
        ClearBackground( BLACK );
        DrawTexture( texture, 0, 0, WHITE );
        DrawFPS( 10, 30 );
        EndDrawing();
    }

    UnloadTexture( texture );
    free( img.data );
    free( buffer );
    CloseWindow();
    return 0;
}
