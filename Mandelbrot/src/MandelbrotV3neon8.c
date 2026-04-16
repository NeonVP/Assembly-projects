#include "raylib.h"
#include <stdlib.h>

#include <arm_neon.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

static void compute_v3_neon( unsigned char *img, float xmin, float xmax, float ymin, float ymax ) {
    float dx = ( xmax - xmin ) / WIDTH;
    float dy = ( ymax - ymin ) / HEIGHT;

    // Просто копируем одно значение сразу во все ячейки вектора
    float32x4_t radius = vdupq_n_f32( L_MAX );
    float32x4_t two = vdupq_n_f32( 2.0f );
    uint32x4_t one = vdupq_n_u32( 1 );
    uint32x4_t max_u8 = vdupq_n_u32( 255 );

    float32x4_t idx0 = { 0.0f, 1.0f, 2.0f, 3.0f };
    float32x4_t idx1 = { 4.0f, 5.0f, 6.0f, 7.0f };
    float32x4_t dx_vec = vdupq_n_f32( dx );

    for ( int y = 0; y < HEIGHT; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * WIDTH;

        int x = 0;
        for ( ; x <= WIDTH - 8; x += 8 ) {
            float base_x = xmin + x * dx;

            // Тут считаем cx сразу для 8 соседних пикселей
            float32x4_t cx0 = vmlaq_f32( vdupq_n_f32( base_x ), idx0, dx_vec );
            float32x4_t cx1 = vmlaq_f32( vdupq_n_f32( base_x ), idx1, dx_vec );

            float32x4_t zx0 = vdupq_n_f32( 0.0f );
            float32x4_t zy0 = vdupq_n_f32( 0.0f );
            float32x4_t zx1 = vdupq_n_f32( 0.0f );
            float32x4_t zy1 = vdupq_n_f32( 0.0f );

            uint32x4_t it0 = vdupq_n_u32( 0 );
            uint32x4_t it1 = vdupq_n_u32( 0 );

            for ( int it = 0; it < MAX_ITER; it++ ) {
                float32x4_t zx2_0 = vmulq_f32( zx0, zx0 );
                float32x4_t zy2_0 = vmulq_f32( zy0, zy0 );
                float32x4_t zx2_1 = vmulq_f32( zx1, zx1 );
                float32x4_t zy2_1 = vmulq_f32( zy1, zy1 );

                // Проверяем, какие точки еще "живые"
                uint32x4_t m0 = vcleq_f32( vaddq_f32( zx2_0, zy2_0 ), radius );
                uint32x4_t m1 = vcleq_f32( vaddq_f32( zx2_1, zy2_1 ), radius );

                if ( ( it & 3 ) == 0 ) {
                    uint32x4_t m = vorrq_u32( m0, m1 );
                    // Если все улетели, дальше считать уже не нужно
                    if ( vaddvq_u32( m ) == 0 )
                        break;
                }

                float32x4_t zxy0 = vmulq_f32( zx0, zy0 );
                float32x4_t zxy1 = vmulq_f32( zx1, zy1 );

                float32x4_t nx0 = vaddq_f32( vsubq_f32( zx2_0, zy2_0 ), cx0 );
                float32x4_t nx1 = vaddq_f32( vsubq_f32( zx2_1, zy2_1 ), cx1 );

                float32x4_t ny0 = vfmaq_f32( cy, zxy0, two );
                float32x4_t ny1 = vfmaq_f32( cy, zxy1, two );

                // Обновляем только те точки, которые еще считаются
                zx0 = vbslq_f32( m0, nx0, zx0 );
                zy0 = vbslq_f32( m0, ny0, zy0 );
                zx1 = vbslq_f32( m1, nx1, zx1 );
                zy1 = vbslq_f32( m1, ny1, zy1 );

                // +1 к итерациям только у активных точек
                it0 = vaddq_u32( it0, vandq_u32( m0, one ) );
                it1 = vaddq_u32( it1, vandq_u32( m1, one ) );
            }

            it0 = vminq_u32( it0, max_u8 );
            it1 = vminq_u32( it1, max_u8 );

            // Ужимаем счетчики до байтов, чтобы быстро записать в картинку
            uint16x4_t it0_16 = vmovn_u32( it0 );
            uint16x4_t it1_16 = vmovn_u32( it1 );
            uint16x8_t it16 = vcombine_u16( it0_16, it1_16 );
            uint8x8_t it8 = vmovn_u16( it16 );
            vst1_u8( &img[row + x], it8 );
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
    InitWindow( WIDTH, HEIGHT, "Mandelbrot v3 neon" );
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

        compute_v3_neon( buffer, xmin, xmax, ymin, ymax );

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
