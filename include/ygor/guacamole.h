/* Copyright (c) 2013-2017, Robert Escriva
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of ygor nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ygor_guacamole_h_
#define ygor_guacamole_h_

/* C */
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

struct guacamole;
struct guacamole* guacamole_create(uint64_t seed);
void guacamole_destroy(struct guacamole* g); /* who would ever want to destroy guacamole? devour? maybe */

void guacamole_seed(struct guacamole* g, uint64_t seed);
void guacamole_generate(struct guacamole* g, void* bytes, size_t bytes_sz);
double guacamole_double(struct guacamole* g);

// draw numbers froma  Zipf distribution with the given parameters
struct guacamole_zipf_params
{
    uint64_t n;
    double alpha;
    double theta;
    double zetan;
    double zeta2;
    double eta;
};
void guacamole_zipf_init_alpha(uint64_t n, double alpha, struct guacamole_zipf_params* p);
void guacamole_zipf_init_theta(uint64_t n, double theta, struct guacamole_zipf_params* p);
uint64_t guacamole_zipf(struct guacamole* g, struct guacamole_zipf_params* p);

// scramble the given value through the specified bijection
// useful for turning zipf output into values spread out in space
struct guacamole_scrambler;
struct guacamole_scrambler* guacamole_scrambler_create(uint64_t bijection);
void guacamole_scrambler_destroy(struct guacamole_scrambler* gs);
void guacamole_scrambler_change(struct guacamole_scrambler* gs, uint64_t bijection);
uint64_t guacamole_scramble(struct guacamole_scrambler* gs, uint64_t value);

// low level 64-bit number to 64-byte output; safe to sequentially increment #
//
// this is derived fromt the salsa encryption scheme, except the key and
// ciphertext were made constant in order to speed up the routine.
void guacamole_mash(uint64_t number, uint32_t output[16]);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* ygor_guacamole_h_ */
