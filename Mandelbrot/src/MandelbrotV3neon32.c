#include "raylib.h"
#include <stdlib.h>

#include <arm_neon.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

static void compute_v3_neon32( unsigned char *img, float xmin, float xmax, float ymin, float ymax ) {
    float dx = ( xmax - xmin ) / WIDTH;
    float dy = ( ymax - ymin ) / HEIGHT;

    // Общие константы.
    float32x4_t radius = vdupq_n_f32( L_MAX );
    float32x4_t two = vdupq_n_f32( 2.0f );
    uint32x4_t one = vdupq_n_u32( 1 );
    uint32x4_t max_u8 = vdupq_n_u32( 255 );
    float32x4_t dx_vec = vdupq_n_f32( dx );

    // Индексы для 32 соседних пикселей: 8 векторов по 4.
    const float32x4_t idx[8] = {
        { 0.0f, 1.0f, 2.0f, 3.0f },      { 4.0f, 5.0f, 6.0f, 7.0f },
        { 8.0f, 9.0f, 10.0f, 11.0f },    { 12.0f, 13.0f, 14.0f, 15.0f },
        { 16.0f, 17.0f, 18.0f, 19.0f },  { 20.0f, 21.0f, 22.0f, 23.0f },
        { 24.0f, 25.0f, 26.0f, 27.0f },  { 28.0f, 29.0f, 30.0f, 31.0f },
    };

    for ( int y = 0; y < HEIGHT; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * WIDTH;

        int x = 0;
        for ( ; x <= WIDTH - 32; x += 32 ) {
            float base_x = xmin + x * dx;

            float32x4_t cx[8];
            float32x4_t zx[8];
            float32x4_t zy[8];
            uint32x4_t itv[8];

            for ( int i = 0; i < 8; i++ ) {
                cx[i] = vmlaq_f32( vdupq_n_f32( base_x ), idx[i], dx_vec );
                zx[i] = vdupq_n_f32( 0.0f );
                zy[i] = vdupq_n_f32( 0.0f );
                itv[i] = vdupq_n_u32( 0 );
            }

            for ( int it = 0; it < MAX_ITER; it++ ) {
                float32x4_t zx2[8];
                float32x4_t zy2[8];
                uint32x4_t mask[8];

                for ( int i = 0; i < 8; i++ ) {
                    zx2[i] = vmulq_f32( zx[i], zx[i] );
                    zy2[i] = vmulq_f32( zy[i], zy[i] );
                    mask[i] = vcleq_f32( vaddq_f32( zx2[i], zy2[i] ), radius );
                }

                if ( ( it & 3 ) == 0 ) {
                    uint32x4_t live = vorrq_u32( vorrq_u32( mask[0], mask[1] ), vorrq_u32( mask[2], mask[3] ) );
                    live = vorrq_u32( live, vorrq_u32( vorrq_u32( mask[4], mask[5] ), vorrq_u32( mask[6], mask[7] ) ) );
                    if ( vaddvq_u32( live ) == 0 )
                        break;
                }

                for ( int i = 0; i < 8; i++ ) {
                    float32x4_t zxy = vmulq_f32( zx[i], zy[i] );
                    float32x4_t nx = vaddq_f32( vsubq_f32( zx2[i], zy2[i] ), cx[i] );
                    float32x4_t ny = vfmaq_f32( cy, zxy, two );

                    zx[i] = vbslq_f32( mask[i], nx, zx[i] );
                    zy[i] = vbslq_f32( mask[i], ny, zy[i] );
                    itv[i] = vaddq_u32( itv[i], vandq_u32( mask[i], one ) );
                }
            }

            uint32_t out[8][4];
            for ( int i = 0; i < 8; i++ ) {
                itv[i] = vminq_u32( itv[i], max_u8 );
                vst1q_u32( out[i], itv[i] );
            }

            for ( int i = 0; i < 4; i++ ) {
                img[row + x + i] = ( unsigned char )out[0][i];
                img[row + x + 4 + i] = ( unsigned char )out[1][i];
                img[row + x + 8 + i] = ( unsigned char )out[2][i];
                img[row + x + 12 + i] = ( unsigned char )out[3][i];
                img[row + x + 16 + i] = ( unsigned char )out[4][i];
                img[row + x + 20 + i] = ( unsigned char )out[5][i];
                img[row + x + 24 + i] = ( unsigned char )out[6][i];
                img[row + x + 28 + i] = ( unsigned char )out[7][i];
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
    if ( iter == 255 )
        return BLACK;
    return ( Color ){ iter * 9, iter * 7, iter * 5, 255 };
}

int main( void ) {
    InitWindow( WIDTH, HEIGHT, "Mandelbrot v3 neon32" );
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

        compute_v3_neon32( buffer, xmin, xmax, ymin, ymax );

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
