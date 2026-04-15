#include "raylib.h"
#include <stdalign.h>
#include <stdlib.h>

#if defined(__AVX__)
#include <immintrin.h>
#endif

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

static void compute_v3_x86( unsigned char *img, float xmin, float xmax, float ymin, float ymax ) {
#if defined(__AVX__)
    float dx = ( xmax - xmin ) / WIDTH;
    float dy = ( ymax - ymin ) / HEIGHT;

    // Просто копируем одно значение сразу во все 8 ячеек вектора.
    __m256 radius = _mm256_set1_ps( L_MAX );
    __m256 two = _mm256_set1_ps( 2.0f );
    __m256 one_ps = _mm256_castsi256_ps( _mm256_set1_epi32( 1 ) );
    // Номера позиций внутри вектора: 0..7.
    __m256 idx = _mm256_set_ps( 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f );

    for ( int y = 0; y < HEIGHT; y++ ) {
        float cy_scalar = ymin + y * dy;
        __m256 cy = _mm256_set1_ps( cy_scalar );
        int row = y * WIDTH;
        int x = 0;

        for ( ; x <= WIDTH - 8; x += 8 ) {
            float base_x = xmin + x * dx;
            // Тут сразу считаем 8 соседних cx за один раз.
            __m256 cx = _mm256_add_ps( _mm256_set1_ps( base_x ), _mm256_mul_ps( idx, _mm256_set1_ps( dx ) ) );
            __m256 zx = _mm256_setzero_ps();
            __m256 zy = _mm256_setzero_ps();
            __m256 iters = _mm256_setzero_ps();

            for ( int it = 0; it < MAX_ITER; it++ ) {
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

            alignas(32) float out_iters[8];
            // Выгружаем 8 счетчиков из вектора в обычный массив.
            _mm256_store_ps( out_iters, iters );
            for ( int k = 0; k < 8; k++ ) {
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
    InitWindow( WIDTH, HEIGHT, "Mandelbrot v3 x86" );
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
