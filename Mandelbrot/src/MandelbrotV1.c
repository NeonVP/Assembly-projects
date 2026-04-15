#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256
#define L_MAX 100.0f

static Color palette( unsigned char iter ) {
    if ( iter == 255 )
        return BLACK;
    return ( Color ){ iter * 9, iter * 7, iter * 5, 255 };
}

static void ComputeV1( unsigned char *img, int width, int height, int max_iter, float xmin,
                           float xmax, float ymin, float ymax ) {
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

int main() {
    InitWindow( WIDTH, HEIGHT, "Mandelbrot (naive)" );
    SetTargetFPS( 600 );

    unsigned char *buffer = calloc( WIDTH * HEIGHT, sizeof( *buffer ) );
    if ( !buffer ) {
        fprintf( stderr, "Buffer allocation failed\n" );
        CloseWindow();
        return 1;
    }

    Image img = { .data = calloc( WIDTH * HEIGHT, sizeof( Color ) ),
                  .width = WIDTH,
                  .height = HEIGHT,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                  .mipmaps = 1 };

    if ( !img.data ) {
        fprintf( stderr, "Image allocation failed\n" );
        free( buffer );
        CloseWindow();
        return 1;
    }

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

        ComputeV1( buffer, WIDTH, HEIGHT, MAX_ITER, xmin, xmax, ymin, ymax );

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
