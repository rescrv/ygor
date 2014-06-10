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

// Sodium
#include <sodium/crypto_stream_salsa208.h>

// STL
#include <memory>
#include <stdexcept>
#include <tr1/random>
#include <vector>

// e
#include <e/endian.h>

// Ygor
#include "ygor.h"
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

////////////////////////////// Class Declarations //////////////////////////////

struct armnod_config
{
    armnod_config();
    enum { NORMAL, UNIFORM, FIXED } method;
    std::string alphabet;
    size_t length_min;
    size_t length_max;
    size_t set_size;

    private:
        armnod_config(const armnod_config&);
        armnod_config& operator = (const armnod_config&);
};

armnod_config :: armnod_config()
    : method(NORMAL)
    , alphabet(alphabet_default)
    , length_min(DEFAULT_LENGTH)
    , length_max(DEFAULT_LENGTH)
    , set_size(DEFAULT_SET_SIZE)
{
}

struct armnod_generator
{
    armnod_generator(const armnod_config* config);
    virtual ~armnod_generator() throw ();

    virtual void seed(uint64_t s) = 0;
    virtual const char* generate_idx(uint64_t idx) = 0;
    virtual const char* generate() = 0;

    const armnod_config* config;

    private:
        armnod_generator(const armnod_generator&);
        armnod_generator& operator = (const armnod_generator&);
};

struct length_generator;

struct armnod_generator_variable_length : public armnod_generator
{
    armnod_generator_variable_length(const armnod_config* config);
    virtual ~armnod_generator_variable_length() throw ();

    virtual void seed(uint64_t s);
    virtual const char* generate_idx(uint64_t idx);
    virtual const char* generate();

    void reset();

    std::vector<char> buffer;
    std::tr1::mt19937 engine;
    std::auto_ptr<length_generator> length_gen;
    std::tr1::uniform_int<size_t> alphabet_dist;
    std::tr1::variate_generator<std::tr1::mt19937, std::tr1::uniform_int<size_t> > alphabet_idx;

    private:
        armnod_generator_variable_length(const armnod_generator_variable_length&);
        armnod_generator_variable_length& operator = (const armnod_generator_variable_length&);
};

struct seekable_engine
{
    typedef uint64_t result_type;
    seekable_engine(uint64_t width);
    uint64_t min() const { return 0; }
    uint64_t max() const { return maxval; }
    uint64_t generate();
    void seek(uint64_t idx);

    private:
        seekable_engine(const seekable_engine&);
        seekable_engine& operator = (const seekable_engine&);
        void next();

        uint64_t width;
        uint64_t maxval;
        uint64_t nonce;
        unsigned char key[crypto_stream_salsa208_KEYBYTES];
        unsigned char bytes[64];
        size_t bytes_idx;
};

struct seekable_engine_wrapper
{
    typedef seekable_engine::result_type result_type;
    seekable_engine_wrapper(seekable_engine* _se) : se(_se) {}
    seekable_engine_wrapper(const seekable_engine_wrapper& other)
        : se(other.se) {}
    uint64_t min() const { return se->min(); }
    uint64_t max() const { return se->max(); }
    uint64_t operator () () { return se->generate(); }
    seekable_engine_wrapper& operator = (const seekable_engine_wrapper& rhs)
    {
        se = rhs.se;
        return *this;
    }

    private:
        seekable_engine* se;
};

struct armnod_generator_fixed : public armnod_generator
{
    armnod_generator_fixed(const armnod_config* config);
    virtual ~armnod_generator_fixed() throw ();

    virtual void seed(uint64_t s);
    virtual const char* generate_idx(uint64_t idx);
    virtual const char* generate();

    void reset();

    uint64_t interval;
    std::vector<char> buffer;
    std::tr1::mt19937 string_chooser_engine;
    std::tr1::uniform_int<size_t> string_chooser_dist;
    std::tr1::variate_generator<std::tr1::mt19937, std::tr1::uniform_int<size_t> > string_chooser;
    std::auto_ptr<seekable_engine> alphabet_engine;
    std::tr1::uniform_int<size_t> alphabet_dist;
    std::tr1::variate_generator<seekable_engine_wrapper, std::tr1::uniform_int<size_t> > alphabet_idx;

    private:
        armnod_generator_fixed(const armnod_generator_fixed&);
        armnod_generator_fixed& operator = (const armnod_generator_fixed&);
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
        ac->method = armnod_config::NORMAL;
    }
    else if (method == "uniform")
    {
        ac->method = armnod_config::UNIFORM;
    }
    else if (method == "fixed")
    {
        ac->method = armnod_config::FIXED;
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
    ac->set_size = size;
    return 0;
}

YGOR_API armnod_generator*
armnod_generator_create(const struct armnod_config* ac)
{
    switch (ac->method)
    {
        case armnod_config::NORMAL:
        case armnod_config::UNIFORM:
            return new (std::nothrow) armnod_generator_variable_length(ac);
        case armnod_config::FIXED:
            return new (std::nothrow) armnod_generator_fixed(ac);
        default:
            return NULL;
    }
}

YGOR_API void
armnod_generator_destroy(struct armnod_generator* ag)
{
    if (ag)
    {
        delete ag;
    }
}

YGOR_API void
armnod_generator_seed(struct armnod_generator* ag, uint64_t s)
{
    return ag->seed(s);
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

/////////////////////////////// Length Generators //////////////////////////////

namespace
{

double
mean(size_t _min, size_t _max)
{
    double min = static_cast<double>(_min);
    double max = static_cast<double>(_max);
    return min + (max - min) / 2;
}

double
sigma(size_t _min, size_t _max)
{
    double min = static_cast<double>(_min);
    double max = static_cast<double>(_max);
    return (max - min) / 6;
}

} // namespace

struct length_generator
{
    length_generator();
    virtual ~length_generator() throw ();
    virtual size_t generate() = 0;

    private:
        length_generator(const length_generator&);
        length_generator& operator = (const length_generator&);
};

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

struct length_generator_uniform : public length_generator
{
    length_generator_uniform(std::tr1::mt19937* eng, size_t min, size_t max);
    virtual ~length_generator_uniform() throw ();
    virtual size_t generate();

    private:
        length_generator_uniform(const length_generator_uniform&);
        length_generator_uniform& operator = (const length_generator_uniform&);
        std::tr1::uniform_int<size_t> m_dist;
        std::tr1::variate_generator<std::tr1::mt19937, std::tr1::uniform_int<size_t> > m_gen;
};

length_generator :: length_generator()
{
}

length_generator :: ~length_generator() throw ()
{
}

length_generator_normal :: length_generator_normal(std::tr1::mt19937* eng, size_t min, size_t max)
    : length_generator()
    , m_dist(mean(min, max), sigma(min, max))
    , m_gen(*eng, m_dist)
    , m_min(min)
    , m_max(max)
{
}

length_generator_normal :: ~length_generator_normal() throw ()
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

length_generator_uniform :: length_generator_uniform(std::tr1::mt19937* eng, size_t min, size_t max)
    : length_generator()
    , m_dist(min, max)
    , m_gen(*eng, m_dist)
{
}

length_generator_uniform :: ~length_generator_uniform() throw ()
{
}

size_t
length_generator_uniform :: generate()
{
    return m_gen();
}

//////////////////////////////// Base Generator ////////////////////////////////

armnod_generator :: armnod_generator(const armnod_config* ac)
    : config(ac)
{
}

armnod_generator :: ~armnod_generator() throw ()
{
}

/////////////////////////// Variable-Length Generator //////////////////////////

armnod_generator_variable_length :: armnod_generator_variable_length(const armnod_config* ac)
    : armnod_generator(ac)
    , buffer()
    , engine()
    , length_gen()
    , alphabet_dist()
    , alphabet_idx(engine, alphabet_dist)
{
    reset();
}

armnod_generator_variable_length :: ~armnod_generator_variable_length() throw ()
{
}

void
armnod_generator_variable_length :: seed(uint64_t s)
{
    engine.seed(s);
    reset();
}

const char*
armnod_generator_variable_length :: generate_idx(uint64_t)
{
    return NULL;
}

const char*
armnod_generator_variable_length :: generate()
{
    size_t length = length_gen->generate();
    assert(length < buffer.size());

    for (size_t i = 0; i < length; ++i)
    {
        size_t c = alphabet_idx();
        assert(c < config->alphabet.size());
        buffer[i] = config->alphabet[c];
    }

    buffer[length] = '\0';
    return &buffer.front();
}

void
armnod_generator_variable_length :: reset()
{
    switch (config->method)
    {
        case armnod_config::NORMAL:
            length_gen.reset(new length_generator_normal(&engine, config->length_min, config->length_max));
            break;
        case armnod_config::UNIFORM:
            length_gen.reset(new length_generator_uniform(&engine, config->length_min, config->length_max));
            break;
        case armnod_config::FIXED:
        default:
            abort();
    }

    buffer.resize(config->length_max + 1);
    alphabet_dist = std::tr1::uniform_int<size_t>(0, config->alphabet.size() - 1);
    alphabet_idx = std::tr1::variate_generator<std::tr1::mt19937, std::tr1::uniform_int<size_t> >(engine, alphabet_dist);
}

//////////////////////////// Fixed-Length Generator ////////////////////////////

seekable_engine :: seekable_engine(uint64_t w)
    : width(w)
    , maxval((1 << (8ULL * width)) - 1)
    , nonce(0)
    , bytes_idx(0)
{
    assert(width > 0);
    assert(width <= 8);
    memset(key, 0, sizeof(key));
    memset(bytes, 0, sizeof(bytes));
    seek(nonce);
}

uint64_t
seekable_engine :: generate()
{
    unsigned char buf[sizeof(uint64_t)];
    memset(buf, 0, sizeof(buf));

    for (size_t i = 0; i < width; )
    {
        if (bytes_idx == 64)
        {
            next();
            assert(bytes_idx < 64);
        }

        buf[i] = bytes[bytes_idx];
        ++i;
        ++bytes_idx;
    }

    uint64_t x = 0;
    e::unpack64le(buf, &x);
    return x;
}

void
seekable_engine :: seek(uint64_t idx)
{
    nonce = idx;
    bytes_idx = 64;
    next();
}

void
seekable_engine :: next()
{
    assert(bytes_idx == 64);
    unsigned char noncebuf[sizeof(uint64_t)];
    e::pack64be(nonce, noncebuf);
    int rc = crypto_stream_salsa208(bytes, sizeof(bytes), noncebuf, key);
    assert(rc == 0);
    bytes_idx = 0;
    ++nonce;
}

armnod_generator_fixed :: armnod_generator_fixed(const armnod_config* ac)
    : armnod_generator(ac)
    , interval(UINT64_MAX / ac->set_size)
    , buffer()
    , string_chooser_engine()
    , string_chooser_dist()
    , string_chooser(string_chooser_engine, string_chooser_dist)
    , alphabet_engine()
    , alphabet_dist()
    , alphabet_idx(seekable_engine_wrapper(NULL), alphabet_dist)
{
}

armnod_generator_fixed :: ~armnod_generator_fixed() throw ()
{
}

void
armnod_generator_fixed :: seed(uint64_t s)
{
    string_chooser_engine.seed(s);
    reset();
}

const char*
armnod_generator_fixed :: generate_idx(uint64_t idx)
{
    idx *= interval;
    alphabet_engine->seek(idx);
    assert(buffer.size() > 0);
    size_t length = buffer.size() - 1;

    for (size_t i = 0; i < length; ++i)
    {
        size_t c = alphabet_idx();
        assert(c < config->alphabet.size());
        buffer[i] = config->alphabet[c];
    }

    buffer[length] = '\0';
    return &buffer.front();
}

const char*
armnod_generator_fixed :: generate()
{
    size_t idx = string_chooser();
    return generate_idx(idx);
}

void
armnod_generator_fixed :: reset()
{
    interval = UINT64_MAX / config->set_size;
    size_t alpha_sz = config->alphabet.size();
    size_t width = 0;

    while (alpha_sz > 0)
    {
        alpha_sz >>= 8;
        ++width;
    }

    width = width > 0 ? width : 1;
    alphabet_engine.reset(new seekable_engine(width));

    assert(config->method == armnod_config::FIXED);
    assert(config->set_size > 0);
    buffer.resize(config->length_max + 1);
    string_chooser_dist = std::tr1::uniform_int<size_t>(0, config->set_size - 1);
    string_chooser = std::tr1::variate_generator<std::tr1::mt19937, std::tr1::uniform_int<size_t> >(string_chooser_engine, string_chooser_dist);
    alphabet_dist = std::tr1::uniform_int<size_t>(0, config->alphabet.size() - 1);
    alphabet_idx = std::tr1::variate_generator<seekable_engine_wrapper, std::tr1::uniform_int<size_t> >(seekable_engine_wrapper(alphabet_engine.get()), alphabet_dist);
}
