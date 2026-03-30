#include <stdio.h>
#include <math.h>

extern void my_printf( const char *format, ... );

int main() {
    int a = 42;
    int b = -100;
    char *str = "Assembly is awesome";

    my_printf( "Hello, World!\n" );
    my_printf( "String: %s | Char: %c\n"
               "%d %s %x %d%%%c%b\n",
               str, 'X', -1, "Love", 3802, 100, 33, 126 );
    my_printf( "Numbers: %d, %d | Hex: %x | Bin: %b\n", a, b, 255, 5 );
    my_printf( "100%% cool code.\n" );
    my_printf( "%d %d %d %d %d %d %d %d %d\n %d %s %x %d%%%c%b\n", 1, 2, 3, 4, 5, 6, 7, 8, 9, -1,
               "Love", 3802, 100, 33, 126 );

    my_printf( "Pi: %f \n", M_PI );

    my_printf( "%f", 1.23e50 );
    my_printf( "0.0: %f\n", 0.0 );
    my_printf( "1.0: %f\n", 1.0 );
    my_printf( "-1.0: %f\n", -1.0 );
    my_printf( "123.456: %f\n", 123.456 );
    my_printf( "-987.654: %f\n", -987.654 );

    return 0;
}