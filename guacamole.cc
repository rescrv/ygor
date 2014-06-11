/*
Derived from salsa208, but chopped up for speed.
Guacamole is a better dip for chips than Salsa.

Code was released under:
version 20080913
D. J. Bernstein
Public domain.
*/

// Ygor
#include "guacamole.h"

#define ROUNDS 8

static uint32_t rotate(uint32_t u,int c)
{
    return (u << c) | (u >> (32 - c));
}

void
guacamole(uint64_t number, uint32_t output[16])
{
    uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
    uint32_t j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;
    int i;

    j0 = x0 = 1634760805;
    j1 = x1 = 0;
    j2 = x2 = 0;
    j3 = x3 = 0;
    j4 = x4 = 0;
    j5 = x5 = 857760878;
    j6 = x6 = number & 0xffffffff;
    j7 = x7 = number >> 32;
    j8 = x8 = 0;
    j9 = x9 = 0;
    j10 = x10 = 2036477234;
    j11 = x11 = 0;
    j12 = x12 = 0;
    j13 = x13 = 0;
    j14 = x14 = 0;
    j15 = x15 = 1797285236;

    for (i = ROUNDS;i > 0;i -= 2)
    {
         x4 ^= rotate( x0+x12, 7);
         x8 ^= rotate( x4+ x0, 9);
        x12 ^= rotate( x8+ x4,13);
         x0 ^= rotate(x12+ x8,18);
         x9 ^= rotate( x5+ x1, 7);
        x13 ^= rotate( x9+ x5, 9);
         x1 ^= rotate(x13+ x9,13);
         x5 ^= rotate( x1+x13,18);
        x14 ^= rotate(x10+ x6, 7);
         x2 ^= rotate(x14+x10, 9);
         x6 ^= rotate( x2+x14,13);
        x10 ^= rotate( x6+ x2,18);
         x3 ^= rotate(x15+x11, 7);
         x7 ^= rotate( x3+x15, 9);
        x11 ^= rotate( x7+ x3,13);
        x15 ^= rotate(x11+ x7,18);
         x1 ^= rotate( x0+ x3, 7);
         x2 ^= rotate( x1+ x0, 9);
         x3 ^= rotate( x2+ x1,13);
         x0 ^= rotate( x3+ x2,18);
         x6 ^= rotate( x5+ x4, 7);
         x7 ^= rotate( x6+ x5, 9);
         x4 ^= rotate( x7+ x6,13);
         x5 ^= rotate( x4+ x7,18);
        x11 ^= rotate(x10+ x9, 7);
         x8 ^= rotate(x11+x10, 9);
         x9 ^= rotate( x8+x11,13);
        x10 ^= rotate( x9+ x8,18);
        x12 ^= rotate(x15+x14, 7);
        x13 ^= rotate(x12+x15, 9);
        x14 ^= rotate(x13+x12,13);
        x15 ^= rotate(x14+x13,18);
    }

    x0 += j0;
    x1 += j1;
    x2 += j2;
    x3 += j3;
    x4 += j4;
    x5 += j5;
    x6 += j6;
    x7 += j7;
    x8 += j8;
    x9 += j9;
    x10 += j10;
    x11 += j11;
    x12 += j12;
    x13 += j13;
    x14 += j14;
    x15 += j15;

    output[0] = x0;
    output[1] = x1;
    output[2] = x2;
    output[3] = x3;
    output[4] = x4;
    output[5] = x5;
    output[6] = x6;
    output[7] = x7;
    output[8] = x8;
    output[9] = x9;
    output[10] = x10;
    output[11] = x11;
    output[12] = x12;
    output[13] = x13;
    output[14] = x14;
    output[15] = x15;
}
