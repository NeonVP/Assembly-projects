#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEED 42

int main( int argc, char *argv[] ) {
    srand( SEED );

    int size = 1500000;
    if ( argc == 2 ) {
        size = atoi( argv[1] );
    }

    printf( "%d\n", size );
    for ( int i = 0; i < size; i++ ) {
        int len = 1 + ( rand() % 128 ); 
        for ( int j = 0; j < len; j++ ) {
            putchar( 'a' + ( rand() % 26 ) );
        }
        putchar( '\n' );
    }
    return 0;
}
