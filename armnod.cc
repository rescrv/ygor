// Copyright (c) 2013-2014, Robert Escriva
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Ygor nor the names of its contributors may be used
//       to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#define __STDC_LIMIT_MACROS

// STL
#include <memory>
#include <stdexcept>
#include <tr1/random>
#include <vector>

// e
#include <e/endian.h>

// Ygor
#include "ygor.h"
#include "guacamole.h"
#include "ygor-internal.h"

#define DEFAULT_LENGTH 16
#define DEFAULT_SET_SIZE 1000

///////////////////////////// Predefined Alphabets /////////////////////////////

static const char alphabet_default[] = "abcdefghijklmnopqrstuvwxyz"
                                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "0123456789!\"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~" " ";
static const char alphabet_alnum[] = "abcdefghijklmnopqrstuvwxyz"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "0123456789";
static const char alphabet_alpha[] = "abcdefghijklmnopqrstuvwxyz"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char alphabet_digit[] = "0123456789";
static const char alphabet_lower[] = "abcdefghijklmnopqrstuvwxyz";
static const char alphabet_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char alphabet_punct[] = "!\"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~";
static const char alphabet_hex[] = "0123456789abcdef";

///////////////////////////// Guacamole-based PRNG /////////////////////////////

struct guacamole_prng
{
    guacamole_prng();
    uint64_t generate(unsigned bits);
    void seek(uint64_t idx);

    private:
        guacamole_prng(const guacamole_prng&);
        guacamole_prng& operator = (const guacamole_prng&);
        void mash();

        uint64_t m_nonce;
        uint64_t m_next;
        unsigned m_next_bits;
        unsigned m_buffer_idx;
        uint32_t m_buffer[16];
};

guacamole_prng :: guacamole_prng()
    : m_nonce()
    , m_next()
    , m_next_bits()
    , m_buffer_idx()
{
    seek(0);
}

uint64_t
guacamole_prng :: generate(unsigned bits)
{
    assert(bits <= 32);

    if (m_buffer_idx == 16)
    {
        mash();
        assert(m_buffer_idx == 0);
    }

    if (m_next_bits < bits)
    {
        uint64_t x = m_buffer[m_buffer_idx];
        m_next |= x << m_next_bits;
        m_next_bits += 32;
        ++m_buffer_idx;
    }

    uint64_t ret = ((1ULL << bits) - 1) & m_next;
    m_next >>= bits;
    m_next_bits -= bits;
    return ret;
}

void
guacamole_prng :: seek(uint64_t idx)
{
    m_nonce = idx;
    m_next = 0;
    m_next_bits = 0;
    m_buffer_idx = 16;
    mash();
}

void
guacamole_prng :: mash()
{
    assert(m_buffer_idx == 16);
    guacamole(m_nonce, m_buffer);
    m_buffer_idx = 0;
    ++m_nonce;
}

/////////////////////////////////// Utilities //////////////////////////////////

namespace
{

double
mean(uint64_t _min, uint64_t _max)
{
    double min = static_cast<double>(_min);
    double max = static_cast<double>(_max);
    return min + (max - min) / 2;
}

double
sigma(uint64_t _min, uint64_t _max)
{
    double min = static_cast<double>(_min);
    double max = static_cast<double>(_max);
    return (max - min) / 6;
}

unsigned
random_bits_for(uint64_t range)
{
    unsigned bits = 0;

    while (1U << bits < range)
    {
        ++bits;
    }

    return bits;
}

double
scale_random_bits(unsigned bits, unsigned range)
{
    double scale = range;
    scale = scale / (1ULL << bits);
    return scale;
}

} // namespace

//////////////////////////////// String Choosers ///////////////////////////////

struct string_chooser
{
    string_chooser() {}
    virtual ~string_chooser() throw () {}

    virtual string_chooser* copy() = 0;
    virtual void seed(uint64_t) = 0;
    virtual bool done() = 0;
    virtual bool has_index() = 0;
    virtual uint64_t index() = 0;
    virtual uint64_t index(uint64_t idx) = 0;

    private:
        string_chooser(const string_chooser&);
        string_chooser& operator = (const string_chooser&);
};

struct string_chooser_arbitrary : public string_chooser
{
    string_chooser_arbitrary() {}
    virtual ~string_chooser_arbitrary() throw () {}

    virtual string_chooser* copy() { return new string_chooser_arbitrary(); }
    virtual void seed(uint64_t) {}
    virtual bool done() { return false; }
    virtual bool has_index() { return false; }
    virtual uint64_t index() { return 0; }
    virtual uint64_t index(uint64_t idx) { return idx; }

    private:
        string_chooser_arbitrary(const string_chooser_arbitrary&);
        string_chooser_arbitrary& operator = (const string_chooser_arbitrary&);
};

struct string_chooser_fixed : public string_chooser
{
    string_chooser_fixed(uint64_t size)
        : m_guacamole(new guacamole_prng())
        , m_set_size(size)
        , m_bits(random_bits_for(size))
        , m_scale(scale_random_bits(m_bits, size))
    {
    }
    virtual ~string_chooser_fixed() throw () {}

    virtual string_chooser* copy() { return new string_chooser_fixed(m_set_size); }
    virtual void seed(uint64_t s) { m_guacamole->seek(s); }
    virtual bool done() { return false; }
    virtual bool has_index() { return true; }
    virtual uint64_t index()
    {
        uint64_t x = m_guacamole->generate(m_bits) * m_scale;
        assert(x < m_set_size);
        return x;
    }
    virtual uint64_t index(uint64_t idx)
    {
        return idx;
    }

    private:
        const std::auto_ptr<guacamole_prng> m_guacamole;
        uint64_t m_set_size;
        unsigned m_bits;
        double m_scale;

        string_chooser_fixed(const string_chooser_fixed&);
        string_chooser_fixed& operator = (const string_chooser_fixed&);
};

struct string_chooser_fixed_once : public string_chooser
{
    string_chooser_fixed_once(uint64_t size) : m_set_size(size), m_idx(0) {}
    virtual ~string_chooser_fixed_once() throw () {}

    virtual string_chooser* copy() { return new string_chooser_fixed_once(m_set_size); }
    virtual void seed(uint64_t) {}
    virtual bool done() { return m_idx >= m_set_size; }
    virtual bool has_index() { return true; }
    virtual uint64_t index() { return m_idx++; }
    virtual uint64_t index(uint64_t idx)
    {
        return idx;
    }

    private:
        uint64_t m_set_size;
        uint64_t m_idx;

        string_chooser_fixed_once(const string_chooser_fixed_once&);
        string_chooser_fixed_once& operator = (const string_chooser_fixed_once&);
};

// The core of string chooser_fixed_zipf reimplements YCSB
// Source: core/src/main/java/com/yahoo/ycsb/generator/ZipfianGenerator.java
// Original License:
//
// Copyright (c) 2010 Yahoo! Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You
// may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License. See accompanying
// LICENSE file.
//
// Author:  Robert Escriva
// Licesne:  Use it under Apache or 3-Clause BSD as advised by your lawyer.
struct string_chooser_fixed_zipf : public string_chooser
{
    string_chooser_fixed_zipf(uint64_t size, double alpha)
        : m_guacamole(new guacamole_prng())
        , m_set_size(size)
        , m_bits(random_bits_for(size))
        , m_alpha(alpha)
        , m_theta(1.0 - (1.0 / m_alpha))
        , m_zetan(zeta(m_set_size, m_theta))
        , m_eta((1 - pow(2.0 / m_set_size, 1 - m_theta)) / (1 - zeta(2, m_theta) / m_zetan))
    {
    }
    virtual ~string_chooser_fixed_zipf() throw () {}

    virtual string_chooser* copy()
    { return new string_chooser_fixed_zipf(m_set_size, m_alpha, m_theta, m_zetan, m_eta); }
    virtual void seed(uint64_t s) { m_guacamole->seek(s); }
    virtual bool done() { return false; }
    virtual bool has_index() { return true; }
    virtual uint64_t index()
    {
        double u = m_guacamole->generate(m_bits);
        u = u / (1U << m_bits);
        assert(u >= 0.0 && u < 1);
        double uz = u * m_zetan;

        if (uz < 1.0)
        {
            return 0;
        }

        if (uz < 1.0 + pow(0.5, m_theta))
        {
            return 1;
        }

        uint64_t idx = m_set_size * pow(m_eta * u - m_eta + 1, m_alpha);
        assert(idx < m_set_size);
        return idx;
    }
    virtual uint64_t index(uint64_t idx)
    {
        return idx;
    }

    private:
        static double zeta(uint64_t n, double theta)
        {
            double sum = 0;

            for (uint64_t i = 0; i < n; ++i)
            {
                sum += 1. / pow(i + 1, theta);
            }

            return sum;
        }
        string_chooser_fixed_zipf(uint64_t size, double alpha,
                                  double theta, double zetan, double eta)
            : m_guacamole(new guacamole_prng())
            , m_set_size(size)
            , m_bits(random_bits_for(size))
            , m_alpha(alpha)
            , m_theta(theta)
            , m_zetan(zetan)
            , m_eta(eta)
        {
        }

        const std::auto_ptr<guacamole_prng> m_guacamole;
        const uint64_t m_set_size;
        const uint64_t m_bits;
        const double m_alpha;
        const double m_theta;
        const double m_zetan;
        const double m_eta;

        string_chooser_fixed_zipf(const string_chooser_fixed_zipf&);
        string_chooser_fixed_zipf& operator = (const string_chooser_fixed_zipf&);
};

//////////////////////////////// Length Choosers ///////////////////////////////

struct length_chooser
{
    length_chooser() {}
    virtual ~length_chooser() throw () {}

    virtual length_chooser* copy() = 0;
    virtual uint64_t max() = 0;
    virtual uint64_t length(guacamole_prng* gp) = 0;

    private:
        length_chooser(const length_chooser&);
        length_chooser& operator = (const length_chooser&);
};

struct length_chooser_constant : public length_chooser
{
    length_chooser_constant(uint64_t sz) : m_sz(sz) {}
    virtual ~length_chooser_constant() throw () {}

    virtual length_chooser* copy() { return new length_chooser_constant(m_sz); }
    virtual uint64_t max() { return m_sz; }
    virtual uint64_t length(guacamole_prng*) { return m_sz; }

    private:
        uint64_t m_sz;

        length_chooser_constant(const length_chooser_constant&);
        length_chooser_constant& operator = (const length_chooser_constant&);
};

struct length_chooser_uniform : public length_chooser
{
    length_chooser_uniform(unsigned _min, unsigned _max)
        : m_min(_min)
        , m_max(_max)
        , m_bits(random_bits_for(_max))
        , m_scale(scale_random_bits(m_bits, _max))
    {
        assert(m_min <= m_max);
    }
    virtual ~length_chooser_uniform() throw () {}

    virtual length_chooser* copy() { return new length_chooser_uniform(m_min, m_max); }
    virtual uint64_t max() { return m_max; }
    virtual uint64_t length(guacamole_prng* gp)
    {
        uint64_t ret = gp->generate(m_bits) * m_scale;
        if (ret < m_min) { ret = m_min; }
        if (ret > m_max) { ret = m_max; }
        return ret;
    }

    private:
        unsigned m_min;
        unsigned m_max;
        unsigned m_bits;
        double m_scale;

        length_chooser_uniform(const length_chooser_uniform&);
        length_chooser_uniform& operator = (const length_chooser_uniform&);
};

///////////////////////// Config and Generator Classes /////////////////////////

struct armnod_config
{
    armnod_config();
    ~armnod_config() throw () {}

    std::string alphabet;
    std::auto_ptr<string_chooser> strings;
    std::auto_ptr<length_chooser> lengths;

    private:
        armnod_config(const armnod_config&);
        armnod_config& operator = (const armnod_config&);
};

armnod_config :: armnod_config()
    : alphabet(alphabet_default)
    , strings(new string_chooser_arbitrary())
    , lengths(new length_chooser_constant(8))
{
}

struct armnod_generator
{
    armnod_generator(const armnod_config* config);
    ~armnod_generator() throw ();

    void seed(uint64_t);
    const char* generate(uint64_t* sz);
    const char* generate_idx(uint64_t idx, uint64_t* sz);
    uint64_t generate_idx_only();

    private:
        const char* generate_from_position(uint64_t* sz);

        const std::auto_ptr<guacamole_prng> m_guacamole;
        const std::auto_ptr<string_chooser> m_strings;
        const std::auto_ptr<length_chooser> m_lengths;
        char* m_buffer;
        char m_alphabet[256] __attribute__ ((aligned (64)));

        armnod_generator(const armnod_generator&);
        armnod_generator& operator = (const armnod_generator&);
};

////////////////////////////////// Public API //////////////////////////////////

extern "C"
{

YGOR_API armnod_config*
armnod_config_create()
{
    return new (std::nothrow) armnod_config();
}

YGOR_API armnod_config*
armnod_config_copy(armnod_config* other)
{
    armnod_config* c = new (std::nothrow) armnod_config();

    if (c)
    {
        c->alphabet = other->alphabet;
        c->strings.reset(other->strings->copy());
        c->lengths.reset(other->lengths->copy());
    }

    return c;
}

YGOR_API void
armnod_config_destroy(armnod_config* ac)
{
    assert(ac);
    delete ac;
}

YGOR_API int
armnod_config_alphabet(struct armnod_config* ac, const char* _chars)
{
    std::string chars(_chars);

    if (chars.size() >= 256)
    {
        return -1;
    }

    ac->alphabet = chars;
    return 0;
}

YGOR_API int
armnod_config_charset(struct armnod_config* ac, const char* _name)
{
    std::string name(_name);

    if (name == "default")
    {
        ac->alphabet = alphabet_default;
    }
    else if (name == "alnum")
    {
        ac->alphabet = alphabet_alnum;
    }
    else if (name == "alpha")
    {
        ac->alphabet = alphabet_alpha;
    }
    else if (name == "digit")
    {
        ac->alphabet = alphabet_digit;
    }
    else if (name == "lower")
    {
        ac->alphabet = alphabet_lower;
    }
    else if (name == "upper")
    {
        ac->alphabet = alphabet_upper;
    }
    else if (name == "punct")
    {
        ac->alphabet = alphabet_punct;
    }
    else if (name == "hex")
    {
        ac->alphabet = alphabet_hex;
    }
    else
    {
        return -1;
    }

    return 0;
}

YGOR_API int
armnod_config_choose_default(struct armnod_config* ac)
{
    assert(ac);
    ac->strings.reset(new string_chooser_arbitrary());
    return 0;
}

YGOR_API int
armnod_config_choose_fixed(struct armnod_config* ac, uint64_t size)
{
    assert(ac);
    ac->strings.reset(new string_chooser_fixed(size));
    return 0;
}

YGOR_API int
armnod_config_choose_fixed_once(struct armnod_config* ac, uint64_t size)
{
    assert(ac);
    ac->strings.reset(new string_chooser_fixed_once(size));
    return 0;
}

YGOR_API int
armnod_config_choose_fixed_zipf(struct armnod_config* ac,
                                uint64_t size, double alpha)
{
    assert(ac);
    ac->strings.reset(new string_chooser_fixed_zipf(size, alpha));
    return 0;
}

YGOR_API int
armnod_config_length_constant(struct armnod_config* ac, uint64_t length)
{
    assert(ac);
    ac->lengths.reset(new length_chooser_constant(length));
    return 0;
}

YGOR_API int
armnod_config_length_uniform(struct armnod_config* ac,
                             uint64_t min, uint64_t max)
{
    assert(ac);
    ac->lengths.reset(new length_chooser_uniform(min, max));
    return 0;
}

YGOR_API armnod_generator*
armnod_generator_create(const struct armnod_config* ac)
{
    return new armnod_generator(ac);
}

YGOR_API void
armnod_generator_destroy(struct armnod_generator* ag)
{
    assert(ag);
    delete ag;
}

YGOR_API void
armnod_seed(struct armnod_generator* ag, uint64_t seed)
{
    assert(ag);
    ag->seed(seed);
}

YGOR_API const char*
armnod_generate(struct armnod_generator* ag)
{
    uint64_t sz;
    return ag->generate(&sz);
}

YGOR_API const char*
armnod_generate_sz(struct armnod_generator* ag, uint64_t* sz)
{
    return ag->generate(sz);
}

YGOR_API const char*
armnod_generate_idx(struct armnod_generator* ag, uint64_t idx)
{
    uint64_t sz;
    return ag->generate_idx(idx, &sz);
}

YGOR_API const char*
armnod_generate_idx_sz(struct armnod_generator* ag,
                       uint64_t idx, uint64_t* sz)
{
    return ag->generate_idx(idx, sz);
}

YGOR_API uint64_t
armnod_generate_idx_only(struct armnod_generator* ag)
{
    return ag->generate_idx_only();
}

} // extern "C"

/////////////////////////// Argparser Implementation ///////////////////////////

struct armnod_argparser_impl : public armnod_argparser
{
    armnod_argparser_impl(const char* prefix, bool method);
    virtual ~armnod_argparser_impl() throw () {}
    virtual const e::argparser& parser() { return ap; }
    virtual armnod_config* config();

    e::argparser ap;
    armnod_config configuration;

    const char* alphabet;
    const char* charset;
    const char* strings;
    long strings_fixed;
    double strings_alpha;
    const char* lengths;
    long lengths_const;
    long lengths_min;
    long lengths_max;

    private:
        armnod_argparser_impl(const armnod_argparser_impl&);
        armnod_argparser_impl& operator = (const armnod_argparser_impl&);
};

YGOR_API armnod_argparser*
armnod_argparser :: create(const char* prefix, bool method)
{
    return new (std::nothrow) armnod_argparser_impl(prefix, method);
}

armnod_argparser :: ~armnod_argparser() throw ()
{
}

armnod_argparser_impl :: armnod_argparser_impl(const char* _prefix, bool method)
    : ap()
    , configuration()
    , alphabet(NULL)
    , charset(NULL)
    , strings("default")
    , strings_fixed(1024)
    , strings_alpha(0.6)
    , lengths("constant")
    , lengths_const(DEFAULT_LENGTH)
    , lengths_min(DEFAULT_LENGTH)
    , lengths_max(DEFAULT_LENGTH)
{
    std::string prefix(_prefix);
    ap.arg().long_name((prefix + "alphabet").c_str())
            .description("alphabet to use for generated strings")
            .metavar("CHARS")
            .as_string(&alphabet);
    ap.arg().long_name((prefix + "charset").c_str())
            .description("charset to use for generated strings")
            .metavar("NAME")
            .as_string(&charset);

    if (method)
    {
        ap.arg().long_name((prefix + "strings").c_str())
                .description("method to use to generate strings (e.g. \"default\", \"fixed\", etc.)")
                .metavar("METHOD")
                .as_string(&strings);
        ap.arg().long_name((prefix + "fixed-size").c_str())
                .description("cardinality of the set of strings that are generated (for methods that support it)")
                .metavar("NUM")
                .as_long(&strings_fixed);
        ap.arg().long_name((prefix + "alpha").c_str())
                .description("the alpha parameter (for Zipf methods)")
                .metavar("A")
                .as_double(&strings_alpha);
    }

    ap.arg().long_name((prefix + "lengths").c_str())
            .description("method to use to select string length (e.g. \"constant\", \"uniform\", etc.)")
            .metavar("METHOD")
            .as_string(&lengths);
    ap.arg().long_name((prefix + "length").c_str())
            .description("length of the generated strings")
            .metavar("NUM")
            .as_long(&lengths_const);
    ap.arg().long_name((prefix + "length-min").c_str())
            .description("minimum length of generated strings")
            .metavar("NUM")
            .as_long(&lengths_min);
    ap.arg().long_name((prefix + "length-max").c_str())
            .description("maximum length of generated strings")
            .metavar("NUM")
            .as_long(&lengths_max);
}

#define FAIL_IF_NEGATIVE(X) do { if ((X) < 0) return NULL; } while (0)

armnod_config*
armnod_argparser_impl :: config()
{
    if (alphabet)
    {
        FAIL_IF_NEGATIVE(armnod_config_alphabet(&configuration, alphabet));
    }

    if (charset)
    {
        FAIL_IF_NEGATIVE(armnod_config_charset(&configuration, charset));
    }

    std::string strings_method(strings);

    if (strings_method == "default")
    {
        FAIL_IF_NEGATIVE(armnod_config_choose_default(&configuration));
    }
    else if (strings_method == "fixed")
    {
        FAIL_IF_NEGATIVE(armnod_config_choose_fixed(&configuration, strings_fixed));
    }
    else if (strings_method == "fixed-once")
    {
        FAIL_IF_NEGATIVE(armnod_config_choose_fixed_once(&configuration, strings_fixed));
    }
    else if (strings_method == "fixed-zipf")
    {
        FAIL_IF_NEGATIVE(armnod_config_choose_fixed_zipf(&configuration, strings_fixed, strings_alpha));
    }
    else
    {
        return NULL;
    }

    std::string lengths_method(lengths);
    lengths_const = std::max(0L, lengths_const);
    lengths_min = std::max(0L, lengths_min);
    lengths_max = std::max(lengths_min, lengths_max);

    if (lengths_method == "constant")
    {
        FAIL_IF_NEGATIVE(armnod_config_length_constant(&configuration, lengths_const));
    }
    else if (lengths_method == "uniform")
    {
        FAIL_IF_NEGATIVE(armnod_config_length_uniform(&configuration, lengths_min, lengths_max));
    }

    return &configuration;
}

/////////////////////////// Generator Implementation ///////////////////////////

#define LENGTH_ROUNDUP 4ULL
#define LENGTH_MASK (LENGTH_ROUNDUP - 1)

armnod_generator :: armnod_generator(const armnod_config* config)
    : m_guacamole(new guacamole_prng())
    , m_strings(config->strings->copy())
    , m_lengths(config->lengths->copy())
    , m_buffer(new char[config->lengths->max() + LENGTH_ROUNDUP * sizeof(char)])
{
    assert(config->alphabet.size() < 256);

    for (uint64_t i = 0; i < 256; ++i)
    {
        double d = i / 256. * config->alphabet.size();
        assert(unsigned(d) < config->alphabet.size());
        m_alphabet[i] = config->alphabet[unsigned(d)];
    }

    memset(m_buffer, 0, (m_lengths->max() + LENGTH_ROUNDUP) * sizeof(char));
}

armnod_generator :: ~armnod_generator() throw ()
{
    if (m_buffer)
    {
        delete[] m_buffer;
    }
}

void
armnod_generator :: seed(uint64_t s)
{
    m_guacamole->seek(s);
    m_strings->seed(s);
}

#pragma GCC diagnostic ignored "-Wunsafe-loop-optimizations"

const char*
armnod_generator :: generate(uint64_t* sz)
{
    if (m_strings->done())
    {
        return NULL;
    }

    if (m_strings->has_index())
    {
        m_guacamole->seek(m_strings->index());
    }

    return generate_from_position(sz);
}

const char*
armnod_generator :: generate_idx(uint64_t idx, uint64_t* sz)
{
    if (m_strings->has_index())
    {
        m_guacamole->seek(m_strings->index(idx));
    }

    return generate_from_position(sz);
}

uint64_t
armnod_generator :: generate_idx_only()
{
    if (m_strings->has_index())
    {
        return m_strings->index();
    }

    return 0;
}

const char*
armnod_generator :: generate_from_position(uint64_t* sz)
{
    uint64_t length = m_lengths->length(m_guacamole.get());
    uint64_t rounded = (length + LENGTH_MASK) & ~LENGTH_MASK;
    assert(length <= rounded && length + LENGTH_ROUNDUP > rounded);
    assert(length <= m_lengths->max());
    assert(length <= m_lengths->max());

    for (uint64_t i = 0; i < rounded; i += LENGTH_ROUNDUP)
    {
        uint32_t x = m_guacamole->generate(32);
        uint32_t a0 = (x) & 255;
        uint32_t a1 = (x >> 8) & 255;
        uint32_t a2 = (x >> 16) & 255;
        uint32_t a3 = (x >> 24) & 255;
        m_buffer[i + 0] = m_alphabet[a0];
        m_buffer[i + 1] = m_alphabet[a1];
        m_buffer[i + 2] = m_alphabet[a2];
        m_buffer[i + 3] = m_alphabet[a3];
    }

    m_buffer[length] = '\0';
    *sz = length;
    return m_buffer;
}
