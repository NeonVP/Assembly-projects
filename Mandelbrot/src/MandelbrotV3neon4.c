#include "raylib.h"
#include <stdlib.h>

#if defined( __aarch64__ ) || defined( __ARM_NEON )
#include <arm_neon.h>
#endif

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

static void compute_v3_neon4( unsigned char *img, float xmin, float xmax, float ymin, float ymax ) {
#if defined( __aarch64__ ) || defined( __ARM_NEON )
    float dx = ( xmax - xmin ) / WIDTH;
    float dy = ( ymax - ymin ) / HEIGHT;

    // Копируем одно значение во все 4 ячейки вектора
    float32x4_t radius = vdupq_n_f32( L_MAX );
    float32x4_t two = vdupq_n_f32( 2.0f );
    uint32x4_t one = vdupq_n_u32( 1 );

    float32x4_t idx = { 0.0f, 1.0f, 2.0f, 3.0f };
    float32x4_t dx_vec = vdupq_n_f32( dx );

    for ( int y = 0; y < HEIGHT; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * WIDTH;

        int x = 0;
        for ( ; x <= WIDTH - 4; x += 4 ) {
            float base_x = xmin + x * dx;
            // Считаем сразу 4 соседних cx
            float32x4_t cx = vmlaq_f32( vdupq_n_f32( base_x ), idx, dx_vec );

            float32x4_t zx = vdupq_n_f32( 0.0f );
            float32x4_t zy = vdupq_n_f32( 0.0f );
            uint32x4_t it = vdupq_n_u32( 0 );

            for ( int k = 0; k < MAX_ITER; k++ ) {
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
                img[row + x + i] = ( unsigned char )( out_iters[i] >= MAX_ITER ? 255 : out_iters[i] );
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
#else
    (void)xmin; (void)xmax; (void)ymin; (void)ymax;
    for ( int i = 0; i < WIDTH * HEIGHT; i++ ) img[i] = 0;
#endif
}

static Color palette( unsigned char iter ) {
    if ( iter == 255 )
        return BLACK;
    return ( Color ){ iter * 9, iter * 7, iter * 5, 255 };
}

int main( void ) {
    InitWindow( WIDTH, HEIGHT, "Mandelbrot v3 neon4" );
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

        compute_v3_neon4( buffer, xmin, xmax, ymin, ymax );

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
