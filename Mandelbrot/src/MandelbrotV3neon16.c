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
    float32x4_t idx2 = { 8.0f, 9.0f, 10.0f, 11.0f };
    float32x4_t idx3 = { 12.0f, 13.0f, 14.0f, 15.0f };
    float32x4_t dx_vec = vdupq_n_f32( dx );

    for ( int y = 0; y < HEIGHT; y++ ) {
        float cy_scalar = ymin + y * dy;
        float32x4_t cy = vdupq_n_f32( cy_scalar );
        int row = y * WIDTH;

        int x = 0;
        for ( ; x <= WIDTH - 16; x += 16 ) {
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

            for ( int it = 0; it < MAX_ITER; it++ ) {
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
