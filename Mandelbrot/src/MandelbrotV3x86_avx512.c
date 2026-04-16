#include "raylib.h"

#include <stdalign.h>
#include <stdlib.h>
#include <stdio.h>
#include <immintrin.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

static void compute_v3_x86( unsigned char *img, float xmin, float xmax, float ymin, float ymax ) {
    static int printed = 0;
    if ( !printed ) { printf("[DEBUG] AVX-512 ENABLED AND RUNNING!\n"); printed = 1; }

    float dx = ( xmax - xmin ) / WIDTH;
    float dy = ( ymax - ymin ) / HEIGHT;

    __m512 radius = _mm512_set1_ps( L_MAX );
    __m512 two = _mm512_set1_ps( 2.0f );
    __m512 one_ps = _mm512_set1_ps( 1.0f );
    __m512 dx_vec = _mm512_set1_ps( dx );

    __m512 idx = _mm512_set_ps( 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f, 8.0f,
                                7.0f,  6.0f,  5.0f,  4.0f,  3.0f,  2.0f,  1.0f, 0.0f );

    for ( int y = 0; y < HEIGHT; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m512 cy = _mm512_set1_ps( cy_scalar );
        int row = y * WIDTH;
        int x = 0;

        for ( ; x <= WIDTH - 16; x += 16 ) {
            float base_x = xmin + x * dx;
            __m512 cx = _mm512_add_ps( _mm512_set1_ps( base_x ), _mm512_mul_ps( idx, dx_vec ) );
            
            __m512 zx = _mm512_setzero_ps();
            __m512 zy = _mm512_setzero_ps();
            __m512 iters = _mm512_setzero_ps();

            for ( int it = 0; it < MAX_ITER; it++ ) {
                __m512 zx2 = _mm512_mul_ps( zx, zx );
                __m512 zy2 = _mm512_mul_ps( zy, zy );
                __m512 radius2 = _mm512_add_ps( zx2, zy2 );
                
                __mmask16 mask = _mm512_cmp_ps_mask( radius2, radius, _CMP_LE_OQ );

                if ( mask == 0 ) break;

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
                img[row + x + k] = ( unsigned char )( iter >= MAX_ITER ? 255 : iter );
            }
        }

        for ( ; x < WIDTH; x++ ) {
            float cx = xmin + x * dx;
            float zx = 0.0f;
            float zy = 0.0f;
            int iter = 0;
            while ( ( zx * zx + zy * zy <= L_MAX ) && ( iter < MAX_ITER ) ) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                float zxy = zx * zy;
                zy = 2.0f * zxy + cy_scalar;
                zx = zx2 - zy2 + cx;
                iter++;
            }
            img[row + x] = ( unsigned char )( iter >= MAX_ITER ? 255 : iter );
        }
    }
}

static Color palette( unsigned char iter ) {
    if ( iter == 255 ) return BLACK;
    return ( Color ){ iter * 9, iter * 7, iter * 5, 255 };
}

int main() {
    InitWindow( WIDTH, HEIGHT, "Mandelbrot v3 x86 (AVX-512)" );
    SetTargetFPS( 1024 );

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

        if ( IsKeyDown( KEY_W ) || IsKeyDown( KEY_UP ) ) centerY -= moveSpeed;
        if ( IsKeyDown( KEY_S ) || IsKeyDown( KEY_DOWN ) ) centerY += moveSpeed;
        if ( IsKeyDown( KEY_A ) || IsKeyDown( KEY_LEFT ) ) centerX -= moveSpeed;
        if ( IsKeyDown( KEY_D ) || IsKeyDown( KEY_RIGHT ) ) centerX += moveSpeed;

        float wheel = GetMouseWheelMove();
        if ( wheel != 0.0f ) scale *= ( wheel > 0.0f ) ? 0.9f : 1.1f;

        float xmin = centerX - scale;
        float xmax = centerX + scale;
        float ymin = centerY - scale * HEIGHT / WIDTH;
        float ymax = centerY + scale * HEIGHT / WIDTH;

        compute_v3_x86( buffer, xmin, xmax, ymin, ymax );

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
