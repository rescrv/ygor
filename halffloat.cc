// Code taken under the Unlicense:
// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org>
//
// Link: http://stackoverflow.com/a/3542975
//
// Changes by Escriva to match convention in ygor

// ygor
#include "halffloat.h"

union float_bits
{
    float f;
    int32_t si;
    uint32_t ui;
};

static int const shift = 13;
static int const shiftSign = 16;

static int32_t const infN = 0x7F800000; // flt32 infinity
static int32_t const maxN = 0x477FE000; // max flt16 normal as a flt32
static int32_t const minN = 0x38800000; // min flt16 normal as a flt32
static int32_t const signN = 0x80000000; // flt32 sign bit

static int32_t const infC = infN >> shift;
static int32_t const nanN = (infC + 1) << shift; // minimum flt16 nan as a flt32
static int32_t const maxC = maxN >> shift;
static int32_t const minC = minN >> shift;
static int32_t const signC = signN >> shiftSign; // flt16 sign bit

static int32_t const mulN = 0x52000000; // (1 << 23) / minN
static int32_t const mulC = 0x33800000; // minN / (1 << (23 - shift))

static int32_t const subC = 0x003FF; // max flt32 subnormal down shifted
static int32_t const norC = 0x00400; // min flt32 normal down shifted

static int32_t const maxD = infC - maxC - 1;
static int32_t const minD = minC - subC - 1;

uint16_t
halffloat_compress(float value)
{
    float_bits v, s;
    v.f = value;
    uint32_t sign = v.si & signN;
    v.si ^= sign;
    sign >>= shiftSign; // logical shift
    s.si = mulN;
    s.si = s.f * v.f; // correct subnormals
    v.si ^= (s.si ^ v.si) & -(minN > v.si);
    v.si ^= (infN ^ v.si) & -((infN > v.si) & (v.si > maxN));
    v.si ^= (nanN ^ v.si) & -((nanN > v.si) & (v.si > infN));
    v.ui >>= shift; // logical shift
    v.si ^= ((v.si - maxD) ^ v.si) & -(v.si > maxC);
    v.si ^= ((v.si - minD) ^ v.si) & -(v.si > subC);
    return v.ui | sign;
}

float
halffloat_decompress(uint16_t value)
{
    float_bits v;
    v.ui = value;
    int32_t sign = v.si & signC;
    v.si ^= sign;
    sign <<= shiftSign;
    v.si ^= ((v.si + minD) ^ v.si) & -(v.si > subC);
    v.si ^= ((v.si + maxD) ^ v.si) & -(v.si > maxC);
    float_bits s;
    s.si = mulC;
    s.f *= v.si;
    int32_t mask = -(norC > v.si);
    v.si <<= shift;
    v.si ^= (s.si ^ v.si) & mask;
    v.si |= sign;
    return v.f;
}
