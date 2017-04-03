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

#ifndef ygor_armnod_h_
#define ygor_armnod_h_

/* C */
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

struct armnod_config;
struct armnod_config* armnod_config_create();
struct armnod_config* armnod_config_copy(struct armnod_config*);
void armnod_config_destroy(struct armnod_config* ac);

/* A c-string containing the alphabet of the random string.
 * Do not make longer than 255 characters (checked)
 */
int armnod_config_alphabet(struct armnod_config* ac, const char* chars);
/* Set the alphabet to one of several presets:
 * default: alnum | punct | " "
 * alnum:   [a-zA-Z0-9]
 * alpha:   [a-zA-Z]
 * digit:   [0-9]
 * lower:   [a-z]
 * upper:   [A-Z]
 * punct:   "!\"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~"
 * hex:     [0-9a-f]
 */
int armnod_config_charset(struct armnod_config* ac, const char* name);

/* Generate random strings that are unlikely to repeat.
 * This is the default behavior.
 */
int armnod_config_choose_default(struct armnod_config* ac);
/* Generate random strings from a fixed size set.
 * The strings will be selected from the fixed-size set uniformly at random, and
 * will repeat indefinitely.
 */
int armnod_config_choose_fixed(struct armnod_config* ac, uint64_t size);
/* Generate random strings from a fixed size set.
 * Each string in the set will be generated exactly once.
 */
int armnod_config_choose_fixed_once(struct armnod_config* ac, uint64_t size);
int armnod_config_choose_fixed_once_slice(struct armnod_config* ac, uint64_t size, uint64_t start, uint64_t limit);
/* Generate random strings from a fixed size set.
 * The strings will be selected from the fixed-size set at random according to a
 * Zipf distribution with parameter "alpha", and will repeat indefinitely.
 */
int armnod_config_choose_fixed_zipf(struct armnod_config* ac,
                                    uint64_t size, double theta);

/* Generate strings of a constant length */
int armnod_config_length_constant(struct armnod_config* ac, uint64_t length);
/* Generate strings of length uniformly distributed between min, max */
int armnod_config_length_uniform(struct armnod_config* ac,
                                 uint64_t min, uint64_t max);

struct armnod_generator;
struct armnod_generator* armnod_generator_create(const struct armnod_config* ac);
void armnod_generator_destroy(struct armnod_generator* ag);

/* Seed the random portion used to select strings */
void armnod_seed(struct armnod_generator* ag, uint64_t seed);
/* Generate a random string using the provided generator.
 * The string will be constructed according to the generator's configuration and
 * the next string in the sequence will be returned.  The returned string points
 * to internal storage and should not be modified or freed.
 *
 * If the generator returns NULL, it means that there are no more strings to
 * return.
 */
const char* armnod_generate(struct armnod_generator* ag);
/* Same as "armnod_generate", but returns the size of the string to avoid a
 * subsequent call to "strlen".
 */
const char* armnod_generate_sz(struct armnod_generator* ag, uint64_t* sz);

#ifdef __cplusplus
} /* extern "C" */

/* e */
#include <e/popt.h>

struct armnod_argparser
{
    static armnod_argparser* create(const char* prefix, bool method=true);
    virtual ~armnod_argparser() throw () = 0;
    virtual const e::argparser& parser() = 0;
    virtual armnod_config* config() = 0;
};

#endif /* __cplusplus */
#endif /* ygor_armnod_h_ */
