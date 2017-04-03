// Copyright (c) 2013-2017, Robert Escriva
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
//     * Neither the name of ygor nor the names of its contributors may be used
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
#include <string>

// ygor
#include <ygor/armnod.h>
#include <ygor/guacamole.h>
#include "visibility.h"

#define DEFAULT_LENGTH 16
#define DEFAULT_SET_SIZE 1000

#define CATCH_BAD_ALLOC_NEG(X) \
    try \
    { \
        X \
    } \
    catch (std::bad_alloc& ba) \
    { \
        errno = ENOMEM; \
        return -1; \
    }

#define CATCH_BAD_ALLOC_NULL(X) \
    try \
    { \
        X \
    } \
    catch (std::bad_alloc& ba) \
    { \
        errno = ENOMEM; \
        return NULL; \
    }

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

//////////////////////////////// String Choosers ///////////////////////////////
// a string_chooser returns a uint64_t indicating the seed to be used for
// generating the next string, and tracks when generation is done

struct string_chooser
{
    string_chooser() {}
    virtual ~string_chooser() throw () {}

    virtual string_chooser* copy() = 0;
    virtual uint64_t seed(guacamole* g) = 0;
    virtual bool done() = 0;

    private:
        string_chooser(const string_chooser&);
        string_chooser& operator = (const string_chooser&);
};

struct string_chooser_fixed : public string_chooser
{
    string_chooser_fixed(uint64_t size) : m_size(size) {}
    virtual ~string_chooser_fixed() throw () {}

    virtual string_chooser* copy() { return new string_chooser_fixed(m_size); }
    virtual uint64_t seed(guacamole* g)
    { double d = guacamole_double(g); return m_size * d; }
    virtual bool done() { return false; }

    private:
        uint64_t m_size;

        string_chooser_fixed(const string_chooser_fixed&);
        string_chooser_fixed& operator = (const string_chooser_fixed&);
};

struct string_chooser_fixed_once : public string_chooser
{
    string_chooser_fixed_once(uint64_t size, uint64_t start, uint64_t limit)
        : m_size(size), m_start(start), m_limit(limit), m_idx(m_start) {}
    virtual ~string_chooser_fixed_once() throw () {}

    virtual string_chooser* copy() { return new string_chooser_fixed_once(m_size, m_start, m_limit); }
    virtual uint64_t seed(guacamole*) { return m_idx++; }
    virtual bool done() { return m_idx >= m_limit; }

    private:
        uint64_t m_size;
        uint64_t m_start;
        uint64_t m_limit;
        uint64_t m_idx;

        string_chooser_fixed_once(const string_chooser_fixed_once&);
        string_chooser_fixed_once& operator = (const string_chooser_fixed_once&);
};

struct string_chooser_fixed_zipf : public string_chooser
{
    string_chooser_fixed_zipf(uint64_t size, double theta)
        : m_zp() { guacamole_zipf_init_theta(size, theta, &m_zp); }
    virtual ~string_chooser_fixed_zipf() throw () {}

    virtual string_chooser* copy() { return new string_chooser_fixed_zipf(m_zp.n, m_zp.theta); }
    virtual uint64_t seed(guacamole* g) { return guacamole_zipf(g, &m_zp); }
    virtual bool done() { return false; }

    private:
        guacamole_zipf_params m_zp;

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
    virtual uint64_t length(guacamole* g) = 0;

    private:
        length_chooser(const length_chooser&);
        length_chooser& operator = (const length_chooser&);
};

struct length_chooser_constant : public length_chooser
{
    length_chooser_constant(uint64_t size) : m_size(size) {}
    virtual ~length_chooser_constant() throw () {}

    virtual length_chooser* copy() { return new length_chooser_constant(m_size); }
    virtual uint64_t max() { return m_size; }
    virtual uint64_t length(guacamole*) { return m_size; }

    private:
        uint64_t m_size;

        length_chooser_constant(const length_chooser_constant&);
        length_chooser_constant& operator = (const length_chooser_constant&);
};

struct length_chooser_uniform : public length_chooser
{
    length_chooser_uniform(uint64_t _min, uint64_t _max)
        : m_min(_min), m_max(_max) { assert(m_min <= m_max); }
    virtual ~length_chooser_uniform() throw () {}

    virtual length_chooser* copy() { return new length_chooser_uniform(m_min, m_max); }
    virtual uint64_t max() { return m_max; }
    virtual uint64_t length(guacamole* g)
    { double d = guacamole_double(g); return m_min + (m_max - m_min) * d; }

    private:
        uint64_t m_min;
        uint64_t m_max;

        length_chooser_uniform(const length_chooser_uniform&);
        length_chooser_uniform& operator = (const length_chooser_uniform&);
};

///////////////////////////////// Configuration ////////////////////////////////

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
    , strings()
    , lengths(new length_chooser_constant(DEFAULT_LENGTH))
{
}

extern "C"
{

YGOR_API armnod_config*
armnod_config_create()
{
    CATCH_BAD_ALLOC_NULL(
    return new armnod_config();
    );
}

YGOR_API armnod_config*
armnod_config_copy(armnod_config* other)
{
    CATCH_BAD_ALLOC_NULL(
    std::auto_ptr<armnod_config> c(new (std::nothrow) armnod_config());

    if (c.get())
    {
        c->alphabet = other->alphabet;

        if (c->strings.get())
        {
            c->strings.reset(other->strings->copy());
        }

        c->lengths.reset(other->lengths->copy());
    }

    return c.release();
    );
}

YGOR_API void
armnod_config_destroy(armnod_config* ac)
{
    delete ac;
}

YGOR_API int
armnod_config_alphabet(struct armnod_config* ac, const char* _chars)
{
    CATCH_BAD_ALLOC_NEG(
    std::string chars(_chars);

    if (chars.size() >= 256)
    {
        return -1;
    }

    ac->alphabet = chars;
    return 0;
    );
}

YGOR_API int
armnod_config_charset(struct armnod_config* ac, const char* _name)
{
    CATCH_BAD_ALLOC_NEG(
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
    );
}

YGOR_API int
armnod_config_choose_default(struct armnod_config* ac)
{
    CATCH_BAD_ALLOC_NEG(
    assert(ac);
    ac->strings.reset();
    return 0;
    );
}

YGOR_API int
armnod_config_choose_fixed(struct armnod_config* ac, uint64_t size)
{
    CATCH_BAD_ALLOC_NEG(
    assert(ac);
    ac->strings.reset(new string_chooser_fixed(size));
    return 0;
    );
}

YGOR_API int
armnod_config_choose_fixed_once(struct armnod_config* ac, uint64_t size)
{
    return armnod_config_choose_fixed_once_slice(ac, size, 0, size);
}

YGOR_API int
armnod_config_choose_fixed_once_slice(struct armnod_config* ac, uint64_t size, uint64_t start, uint64_t limit)
{
    CATCH_BAD_ALLOC_NEG(
    assert(ac);
    ac->strings.reset(new string_chooser_fixed_once(size, start, limit));
    return 0;
    );
}

YGOR_API int
armnod_config_choose_fixed_zipf(struct armnod_config* ac,
                                uint64_t size, double alpha)
{
    CATCH_BAD_ALLOC_NEG(
    assert(ac);
    ac->strings.reset(new string_chooser_fixed_zipf(size, alpha));
    return 0;
    );
}

YGOR_API int
armnod_config_length_constant(struct armnod_config* ac, uint64_t length)
{
    CATCH_BAD_ALLOC_NEG(
    assert(ac);
    ac->lengths.reset(new length_chooser_constant(length));
    return 0;
    );
}

YGOR_API int
armnod_config_length_uniform(struct armnod_config* ac,
                             uint64_t min, uint64_t max)
{
    CATCH_BAD_ALLOC_NEG(
    assert(ac);
    ac->lengths.reset(new length_chooser_uniform(min, max));
    return 0;
    );
}

} // extern "C"

/////////////////////////////////// Generator //////////////////////////////////

struct armnod_generator
{
    armnod_generator();
    ~armnod_generator() throw ();

    bool init(const armnod_config* config);
    void seed(uint64_t seed) { guacamole_seed(m_random, seed); }
    const char* generate(size_t* sz);

    private:
        guacamole* m_random;
        guacamole* m_seeded;
        std::auto_ptr<string_chooser> m_strings;
        std::auto_ptr<length_chooser> m_lengths;
        char m_alphabet[256] __attribute__ ((aligned (64)));
        char* m_buffer;

        armnod_generator(const armnod_generator&);
        armnod_generator& operator = (const armnod_generator&);
};

#define LENGTH_ROUNDUP 4ULL
#define LENGTH_MASK (LENGTH_ROUNDUP - 1)

armnod_generator :: armnod_generator()
    : m_random(NULL)
    , m_seeded(NULL)
    , m_strings()
    , m_lengths()
    , m_buffer(NULL)
{
}

armnod_generator :: ~armnod_generator() throw ()
{
    if (m_random) guacamole_destroy(m_random);
    if (m_seeded) guacamole_destroy(m_seeded);
    if (m_buffer) delete m_buffer;
}

bool
armnod_generator :: init(const armnod_config* config)
{
    m_random = guacamole_create(0);
    m_seeded = guacamole_create(0);

    if (!m_random || !m_seeded)
    {
        return false;
    }

    if (config->strings.get())
    {
        m_strings.reset(config->strings->copy());
    }

    m_lengths.reset(config->lengths->copy());
    m_buffer = new char[m_lengths->max() + LENGTH_ROUNDUP * sizeof(char)];
    assert(config->alphabet.size() < 256);

    for (uint64_t i = 0; i < 256; ++i)
    {
        double d = i / 256. * config->alphabet.size();
        assert(unsigned(d) < config->alphabet.size());
        m_alphabet[i] = config->alphabet[unsigned(d)];
    }

    memset(m_buffer, 0, (m_lengths->max() + LENGTH_ROUNDUP) * sizeof(char));
    return true;
}

#pragma GCC diagnostic ignored "-Wunsafe-loop-optimizations"

const char*
armnod_generator :: generate(size_t* sz)
{
    if (m_strings.get())
    {
        if (m_strings->done())
        {
            return NULL;
        }

        uint64_t seed = m_strings->seed(m_random);
        guacamole_seed(m_seeded, seed);
    }

    uint64_t length = m_lengths->length(m_seeded);
    uint64_t rounded = (length + LENGTH_MASK) & ~LENGTH_MASK;
    assert(length <= rounded && length + LENGTH_ROUNDUP > rounded);
    assert(length <= m_lengths->max());
    assert(length <= m_lengths->max());
    guacamole_generate(m_seeded, m_buffer, rounded);

    for (uint64_t i = 0; i < rounded; i += LENGTH_ROUNDUP)
    {
        m_buffer[i + 0] = m_alphabet[(unsigned char)m_buffer[i + 0]];
        m_buffer[i + 1] = m_alphabet[(unsigned char)m_buffer[i + 1]];
        m_buffer[i + 2] = m_alphabet[(unsigned char)m_buffer[i + 2]];
        m_buffer[i + 3] = m_alphabet[(unsigned char)m_buffer[i + 3]];
    }

    m_buffer[length] = '\0';
    *sz = length;
    return m_buffer;
}

YGOR_API armnod_generator*
armnod_generator_create(const struct armnod_config* ac)
{
    CATCH_BAD_ALLOC_NULL(
    std::auto_ptr<armnod_generator> ag(new armnod_generator());

    if (ag->init(ac))
    {
        return ag.release();
    }
    else
    {
        return NULL;
    }
    );
}

YGOR_API void
armnod_generator_destroy(struct armnod_generator* ag)
{
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
    long strings_fixed_start;
    long strings_fixed_limit;
    bool strings_fixed_start_set;
    bool strings_fixed_limit_set;
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
    , strings_fixed_start(0)
    , strings_fixed_limit(0)
    , strings_fixed_start_set(false)
    , strings_fixed_limit_set(false)
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
        ap.arg().long_name((prefix + "fixed-start").c_str())
                .description("starting index for the fixed-size method")
                .metavar("NUM")
                .as_long(&strings_fixed_start)
                .set_true(&strings_fixed_start_set);
        ap.arg().long_name((prefix + "fixed-limit").c_str())
                .description("ending index for the fixed-size method")
                .metavar("NUM")
                .as_long(&strings_fixed_limit)
                .set_true(&strings_fixed_limit_set);
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
        uint64_t size = strings_fixed;
        uint64_t start = strings_fixed_start_set ? strings_fixed_start : 0;
        uint64_t limit = strings_fixed_limit_set ? strings_fixed_limit : strings_fixed;

        if (limit > size)
        {
            limit = size;
        }

        if (start > limit)
        {
            start = limit;
        }

        FAIL_IF_NEGATIVE(armnod_config_choose_fixed_once_slice(&configuration, size, start, limit));
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
