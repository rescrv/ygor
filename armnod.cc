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

/////////////////////////////// Internal Classes ///////////////////////////////

enum distribution_t
{
    DIST_NORMAL,
    DIST_UNIFORM,
    DIST_FIXED
};

struct armnod_config
{
    armnod_config();

    distribution_t method;
    std::string alphabet;
    size_t length_min;
    size_t length_max;
    size_t fixed_set_size;

    private:
        armnod_config(const armnod_config&);
        armnod_config& operator = (const armnod_config&);
};

armnod_config :: armnod_config()
    : method(DIST_NORMAL)
    , alphabet(alphabet_default)
    , length_min(DEFAULT_LENGTH)
    , length_max(DEFAULT_LENGTH)
    , fixed_set_size(DEFAULT_SET_SIZE)
{
}

// The string chooser selects the position for the prng before generating each
// string.  It will likely have its own prng.
struct string_chooser
{
    static string_chooser* create(const armnod_config*);

    string_chooser();
    virtual ~string_chooser() throw ();

    virtual uint64_t index() = 0;

    private:
        string_chooser(const string_chooser&);
        string_chooser& operator = (const string_chooser&);
};

// The length chooser returns the length of the string deterministically from
// the provided prng.  A length chooser constructed with the same arguments, and
// provided a prng at the same position in the stream, should return the same
// length.
struct length_chooser
{
    static length_chooser* create(const armnod_config*);

    length_chooser();
    virtual ~length_chooser() throw ();

    virtual uint64_t max() = 0;
    virtual uint64_t length(guacamole_prng* gp) = 0;

    private:
        length_chooser(const length_chooser&);
        length_chooser& operator = (const length_chooser&);
};

struct armnod_generator
{
    armnod_generator(const armnod_config* config);
    ~armnod_generator() throw ();

    const char* generate_idx(uint64_t idx);
    const char* generate();

    private:
        const char* generate_from_current_position();

        const std::auto_ptr<guacamole_prng> m_guacamole;
        const std::auto_ptr<string_chooser> m_strings;
        const std::auto_ptr<length_chooser> m_lengths;
        char* m_buffer;
        char m_alphabet[1U << 8] __attribute__ ((aligned (64)));

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

YGOR_API void
armnod_config_destroy(armnod_config* ac)
{
    if (ac)
    {
        delete ac;
    }
}

YGOR_API int
armnod_config_method(struct armnod_config* ac, const char* _method)
{
    std::string method(_method);

    if (method == "normal")
    {
        ac->method = DIST_NORMAL;
    }
    else if (method == "uniform")
    {
        ac->method = DIST_UNIFORM;
    }
    else if (method == "fixed")
    {
        ac->method = DIST_FIXED;
    }
    else
    {
        return -1;
    }

    return 0;
}

YGOR_API int
armnod_config_alphabet(struct armnod_config* ac, const char* chars)
{
    ac->alphabet = std::string(chars);
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
armnod_config_length(struct armnod_config* ac, size_t len)
{
    ac->length_min = len;
    ac->length_max = len;
    return 0;
}

YGOR_API int
armnod_config_length_min(struct armnod_config* ac, size_t len)
{
    ac->length_min = len;
    ac->length_max = std::max(ac->length_max, ac->length_min);
    return 0;
}

YGOR_API int
armnod_config_length_max(struct armnod_config* ac, size_t len)
{
    ac->length_max = len;
    ac->length_min = std::min(ac->length_max, ac->length_min);
    return 0;
}

YGOR_API int
armnod_config_set_size(struct armnod_config* ac, size_t size)
{
    ac->fixed_set_size = size;
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
    if (ag)
    {
        delete ag;
    }
}

YGOR_API const char*
armnod_generate_idx(struct armnod_generator* ag, uint64_t idx)
{
    return ag->generate_idx(idx);
}

YGOR_API const char*
armnod_generate(struct armnod_generator* ag)
{
    return ag->generate();
}

} // extern "C"

/////////////////////////// Argparser Implementation ///////////////////////////

struct armnod_argparser_impl : public armnod_argparser
{
    armnod_argparser_impl(const char* prefix);
    virtual ~armnod_argparser_impl() throw () {}
    virtual const e::argparser& parser() { return ap; }
    virtual armnod_config* config();

    e::argparser ap;
    armnod_config configuration;
    const char* alphabet_value;
    const char* charset_value;
    bool length_value_trip;
    long length_value;
    bool length_min_value_trip;
    long length_min_value;
    bool length_max_value_trip;
    long length_max_value;

    private:
        armnod_argparser_impl(const armnod_argparser_impl&);
        armnod_argparser_impl& operator = (const armnod_argparser_impl&);
};

YGOR_API armnod_argparser*
armnod_argparser :: create(const char* prefix)
{
    return new (std::nothrow) armnod_argparser_impl(prefix);
}

armnod_argparser :: ~armnod_argparser() throw ()
{
}

armnod_argparser_impl :: armnod_argparser_impl(const char* _prefix)
    : ap()
    , configuration()
    , alphabet_value(NULL)
    , charset_value(NULL)
    , length_value_trip(false)
    , length_value(DEFAULT_LENGTH)
    , length_min_value_trip(false)
    , length_min_value(DEFAULT_LENGTH)
    , length_max_value_trip(false)
    , length_max_value(DEFAULT_LENGTH)
{
    std::string prefix(_prefix);
    ap.arg().long_name((prefix + "alphabet").c_str())
            .description("alphabet to use for generated strings")
            .metavar("CHARS")
            .as_string(&alphabet_value);
    ap.arg().long_name((prefix + "charset").c_str())
            .description("charset to use for generated strings")
            .metavar("NAME")
            .as_string(&charset_value);
    ap.arg().long_name((prefix + "length").c_str())
            .description("length of generated strings")
            .metavar("NUM")
            .as_long(&length_value).set_true(&length_value_trip);
    ap.arg().long_name((prefix + "length-min").c_str())
            .description("minimum length of generated strings")
            .metavar("NUM")
            .as_long(&length_min_value).set_true(&length_min_value_trip);
    ap.arg().long_name((prefix + "length-max").c_str())
            .description("maximum length of generated strings")
            .metavar("NUM")
            .as_long(&length_max_value).set_true(&length_max_value_trip);
}

armnod_config*
armnod_argparser_impl :: config()
{
    length_min_value = std::max(static_cast<long int>(0), length_min_value);
    length_max_value = std::max(length_min_value, length_max_value);

    if (alphabet_value)
    {
        armnod_config_alphabet(&configuration, alphabet_value);
    }

    if (charset_value)
    {
        armnod_config_charset(&configuration, charset_value);
    }

    if (length_value_trip)
    {
        armnod_config_length(&configuration, length_value);
    }
    else
    {
        if (length_min_value_trip)
        {
            armnod_config_length_min(&configuration, length_min_value);
        }

        if (length_max_value_trip)
        {
            armnod_config_length_max(&configuration, length_max_value);
        }
    }

    return &configuration;
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

    while (1 << bits < range)
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

string_chooser :: string_chooser()
{
}

string_chooser :: ~string_chooser() throw ()
{
}

struct string_chooser_arbitrary : public string_chooser
{
    string_chooser_arbitrary() {}
    virtual ~string_chooser_arbitrary() throw () {}

    virtual uint64_t index() { return UINT64_MAX; }
};

string_chooser*
string_chooser::create(armnod_config const*)
{
    return new string_chooser_arbitrary();
}

//////////////////////////////// Length Choosers ///////////////////////////////

length_chooser :: length_chooser()
{
}

length_chooser :: ~length_chooser() throw ()
{
}

struct length_chooser_constant : public length_chooser
{
    length_chooser_constant(uint64_t sz) : m_sz(sz) {}
    virtual ~length_chooser_constant() throw ()
    {
    }

    virtual uint64_t max() { return m_sz; }
    virtual uint64_t length(guacamole_prng* gp) { return m_sz; }

    private:
        uint64_t m_sz;
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
    virtual ~length_chooser_uniform() throw ()
    {
    }

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
};

struct length_chooser_normal : public length_chooser
{
    length_chooser_normal(unsigned min, unsigned max)
        : m_min(min)
        , m_max(max)
    {
        assert(m_min <= m_max);
    }
    virtual ~length_chooser_normal() throw ()
    {
    }

    virtual uint64_t length(guacamole_prng* gp);

    private:
        unsigned m_min;
        unsigned m_max;
};

#if 0
struct length_generator_normal : public length_generator
{
    length_generator_normal(std::tr1::mt19937* eng, size_t min, size_t max);
    virtual ~length_generator_normal() throw ();
    virtual size_t generate();

    private:
        length_generator_normal(const length_generator_normal&);
        length_generator_normal& operator = (const length_generator_normal&);
        std::tr1::normal_distribution<double> m_dist;
        std::tr1::variate_generator<std::tr1::mt19937, std::tr1::normal_distribution<double> > m_gen;
        size_t m_min;
        size_t m_max;
};

length_generator_normal :: length_generator_normal(std::tr1::mt19937* eng, size_t min, size_t max)
    : length_generator()
    , m_dist(mean(min, max), sigma(min, max))
    , m_gen(*eng, m_dist)
    , m_min(min)
    , m_max(max)
{
}

size_t
length_generator_normal :: generate()
{
    size_t x = static_cast<size_t>(m_gen());
    if (x < m_min) { x = m_min; }
    if (x > m_max) { x = m_max; }
    return x;
}
#endif

length_chooser*
length_chooser::create(armnod_config const*)
{
    return new length_chooser_constant(16);
}

/////////////////////////// Generator Implementation ///////////////////////////

#define LENGTH_ROUNDUP 4ULL
#define LENGTH_MASK (LENGTH_ROUNDUP - 1)

armnod_generator :: armnod_generator(const armnod_config* config)
    : m_guacamole(new guacamole_prng())
    , m_strings(string_chooser::create(config))
    , m_lengths(length_chooser::create(config))
    , m_buffer(new char[m_lengths->max() + LENGTH_ROUNDUP])
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

const char *
armnod_generator :: generate_idx(uint64_t idx)
{
    m_guacamole->seek(idx);
    return generate_from_current_position();
}

const char*
armnod_generator :: generate()
{
    uint64_t idx = m_strings->index();

    if (idx != UINT64_MAX)
    {
        m_guacamole->seek(idx);
    }

    return generate_from_current_position();
}

const char*
armnod_generator :: generate_from_current_position()
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
    return m_buffer;
}
