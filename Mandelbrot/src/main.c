#include "mandelbrot.h"

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 600
#define MAX_ITER 256

static Color palette( unsigned char iter ) {
    if ( iter == 255 )
        return BLACK;
    return ( Color ){ iter * 9, iter * 7, iter * 5, 255 };
}

static void print_usage( const char *prog ) {
    printf( "Usage: %s [v1|v2|v3_neon|v3_x86]\n", prog );
    printf( "Aliases: naive, arrayed, neon, x86\n" );
}

int main( int argc, char **argv ) {
    MandelbrotImpl impl = MANDEL_IMPL_V1;

    if ( argc > 1 ) {
        if ( strcmp( argv[1], "--help" ) == 0 || strcmp( argv[1], "-h" ) == 0 ) {
            print_usage( argv[0] );
            return 0;
        }
        if ( !mandelbrot_impl_from_string( argv[1], &impl ) ) {
            fprintf( stderr, "Unknown implementation: %s\n", argv[1] );
            print_usage( argv[0] );
            return 1;
        }
    }

    char title[128] = {};
    snprintf( title, sizeof( title ), "Mandelbrot (%s)", mandelbrot_impl_name( impl ) );

    InitWindow( WIDTH, HEIGHT, title );
    SetTargetFPS( 300 );

    unsigned char *buffer = calloc( WIDTH * HEIGHT, sizeof( *buffer ) );
    if ( !buffer ) {
        fprintf( stderr, "buffer allocation failed\n" );
        CloseWindow();
        return 1;
    }

    Image img = { .data = malloc( WIDTH * HEIGHT * sizeof( Color ) ),
                  .width = WIDTH,
                  .height = HEIGHT,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                  .mipmaps = 1 };

    if ( !img.data ) {
        fprintf( stderr, "image allocation failed\n" );
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

        mandelbrot_compute( buffer, WIDTH, HEIGHT, MAX_ITER, xmin, xmax, ymin, ymax, impl );

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
