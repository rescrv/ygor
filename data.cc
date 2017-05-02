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

// C
#include <math.h>
#include <stdio.h>
#include <string.h>

// STL
#include <list>
#include <vector>

// po6
#include <po6/threads/mutex.h>

// e
#include <e/ao_hash_map.h>
#include <e/endian.h>
#include <e/guard.h>
#include <e/lookup3.h>
#include <e/varint.h>

// ygor
#include <ygor/data.h>
#include <ygor/guacamole.h>
#include "halffloat.h"
#include "ygor-internal.h"
#include "visibility.h"

template<typename T>
uint64_t
hash_ptr(T* t)
{
    return e::lookup3_64(reinterpret_cast<intptr_t>(t));
}

#define MAX_POINT_SIZE 20
#define SERIES_BUFFER_SIZE 1024

YGOR_API int
ygor_is_precise(ygor_precision p)
{
    switch(p)
    {
        case YGOR_PRECISE_INTEGER: return true;
        case YGOR_HALF_PRECISION: return false;
        case YGOR_SINGLE_PRECISION: return false;
        case YGOR_DOUBLE_PRECISION: return false;
        default: return false;
    }
}

ygor_precision
approximate_precision(ygor_precision p)
{
    switch(p)
    {
        case YGOR_PRECISE_INTEGER:
        case YGOR_DOUBLE_PRECISION:
            return YGOR_DOUBLE_PRECISION;
        case YGOR_SINGLE_PRECISION:
            return YGOR_SINGLE_PRECISION;
        case YGOR_HALF_PRECISION:
        default:
            return YGOR_HALF_PRECISION;
    }
}

typedef bool (*sort_func_t)(const ygor_data_point& lhs, const ygor_data_point& rhs);
typedef unsigned char* (*pack_func_t)(const ygor_data_point* prev, ygor_data_point* point, unsigned char* out);
typedef const unsigned char* (*unpack_func_t)(const unsigned char* in, const unsigned char* end, const ygor_data_point* prev, ygor_data_point* point);

struct ygor_series_logger
{
    static sort_func_t sort_func(const ygor_series* s);
    static pack_func_t pack_func(const ygor_series* s);

    ygor_series_logger(ygor_data_logger* dl, const ygor_series* s);
    ~ygor_series_logger() throw ();

    int record(ygor_data_point* ydp);
    int flush();
    // call holding io_mtx;
    int write(ygor_data_point* points, size_t points_sz);

    ygor_data_logger* ydl;
    const ygor_series* ys;
    size_t sindex;
    // the series logger uses a double-buffer flip-flop
    // While one set of points are being filled in, the other is being written
    // out to the data logger.  The goal is for the buffering to never get to a
    // state that it is waiting on the data to be written out.  The "points"
    // array will point to either points_A or points_B.
    po6::threads::mutex points_mtx;
    ygor_data_point points_A[SERIES_BUFFER_SIZE];
    ygor_data_point points_B[SERIES_BUFFER_SIZE];
    ygor_data_point* points;
    size_t points_sz;
    // The I/O path takes the in memory representation and compacts it according
    // to the series description
    po6::threads::mutex io_mtx;
    sort_func_t sort;
    pack_func_t pack;

    private:
        ygor_series_logger(const ygor_series_logger&);
        ygor_series_logger& operator = (const ygor_series_logger&);
};

struct ygor_data_logger
{
    ygor_data_logger();
    ~ygor_data_logger() throw ();

    bool init(const char* output,
              const ygor_series** series,
              size_t series_sz);
    size_t series_index(const ygor_series* s);
    ygor_series_logger* get_series_logger(const ygor_series* s);

    po6::threads::mutex output_mtx;
    FILE* output;
    const ygor_series** series;
    size_t series_sz;
    e::ao_hash_map<const ygor_series*, ygor_series_logger*, hash_ptr, (ygor_series*)NULL> series_loggers;

    private:
        ygor_data_logger(const ygor_data_logger&);
        ygor_data_logger& operator = (const ygor_data_logger&);
};

YGOR_API ygor_data_logger*
ygor_data_logger_create(const char* output,
                        const ygor_series** series,
                        size_t series_sz)
{
    ygor_data_logger* ydl = new ygor_data_logger();

    if (!ydl->init(output, series, series_sz))
    {
        delete ydl;
        return NULL;
    }

    return ydl;
}

YGOR_API int
ygor_data_logger_flush_and_destroy(ygor_data_logger* ydl)
{
    bool success = true;

    for (size_t i = 0; i < ydl->series_sz; ++i)
    {
        ygor_series_logger* ysl = ydl->get_series_logger(ydl->series[i]);

        if (!ysl || ysl->flush() < 0)
        {
            success = false;
        }
    }

    ydl->output_mtx.lock();
    int x = fflush(ydl->output);
    int y = fclose(ydl->output);
    ydl->output_mtx.unlock();
    delete ydl;

    if (!success || x != 0 || y != 0)
    {
        return -1;
    }

    return 0;
}

YGOR_API int
ygor_data_logger_record(ygor_data_logger* ydl, ygor_data_point* ydp)
{
    ygor_series_logger* ysl = ydl->get_series_logger(ydp->series);

    if (!ysl)
    {
        return -1;
    }

    return ysl->record(ydp);
}

ygor_data_logger :: ygor_data_logger()
    : output_mtx()
    , output(NULL)
    , series(NULL)
    , series_sz(0)
    , series_loggers()
{
}

ygor_data_logger :: ~ygor_data_logger() throw ()
{
    for (size_t i = 0; i < series_sz; ++i)
    {
        delete get_series_logger(series[i]);
    }

    delete[] series;
    // do not touch output because we only delete ydl from flush_and_destroy,
    // and it would be an error to double fclose it.
}

bool
ygor_data_logger :: init(const char* output_name,
                         const ygor_series** s, size_t s_sz)
{
    series = new const ygor_series*[s_sz];

    for (size_t i = 0; i < s_sz; ++i)
    {
        series[i] = s[i];
    }

    series_sz = s_sz;
    po6::threads::mutex::hold hold(&output_mtx);
    output = fopen(output_name, "w");

    for (size_t i = 0; i < s_sz; ++i)
    {
        ygor_series_logger* ysl = new ygor_series_logger(this, series[i]);

        if (!series_loggers.put(series[i], ysl))
        {
            return false;
        }

        size_t name_len = strlen(series[i]->name);

        if (name_len > 64 ||
            fwrite(series[i]->name, 1, name_len + 1, output) != name_len + 1)
        {
            return false;
        }

        unsigned char buf[4];
        buf[0] = series[i]->indep_units;
        buf[1] = series[i]->indep_precision;
        buf[2] = series[i]->dep_units;
        buf[3] = series[i]->dep_precision;

        if (fwrite(buf, 1, 4, output) != 4)
        {
            return false;
        }
    }

    if (fwrite("\x00", 1, 1, output) != 1)
    {
        return false;
    }

    return true;
}

size_t
ygor_data_logger :: series_index(const ygor_series* s)
{
    for (size_t i = 0; i < series_sz; ++i)
    {
        if (series[i] == s)
        {
            return i;
        }
    }

    return series_sz;
}

ygor_series_logger*
ygor_data_logger :: get_series_logger(const ygor_series* s)
{
    ygor_series_logger* ysl = NULL;
    return series_loggers.get(s, &ysl) ? ysl : NULL;
}

bool
sort_func_precise(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.indep.precise < rhs.indep.precise;
}

bool
sort_func_approximate(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.indep.approximate < rhs.indep.approximate;
}

sort_func_t
ygor_series_logger :: sort_func(const ygor_series* s)
{
    switch (s->indep_precision)
    {
        case YGOR_PRECISE_INTEGER: return sort_func_precise;
        case YGOR_HALF_PRECISION: return sort_func_approximate;
        case YGOR_SINGLE_PRECISION: return sort_func_approximate;
        case YGOR_DOUBLE_PRECISION: return sort_func_approximate;
        default: return NULL;
    }
}

template <ygor_precision P>
unsigned char* pack_value_templ(const ygor_data_value& v, unsigned char* out);

template <>
unsigned char*
pack_value_templ<YGOR_PRECISE_INTEGER>(const ygor_data_value& v, unsigned char* out)
{
    return e::packvarint64(v.precise, out);
}

template <>
unsigned char*
pack_value_templ<YGOR_HALF_PRECISION>(const ygor_data_value& v, unsigned char* out)
{
    uint16_t h = halffloat_compress(v.approximate);
    return e::pack16be(h, out);
}

template <>
unsigned char*
pack_value_templ<YGOR_SINGLE_PRECISION>(const ygor_data_value& v, unsigned char* out)
{
    return e::packfloatbe(v.approximate, out);
}

template <>
unsigned char*
pack_value_templ<YGOR_DOUBLE_PRECISION>(const ygor_data_value& v, unsigned char* out)
{
    return e::packdoublebe(v.approximate, out);
}

template <ygor_precision I, ygor_precision D>
unsigned char*
pack_func_templ(const ygor_data_point*, ygor_data_point* point, unsigned char* out)
{
    out = pack_value_templ<I>(point->indep, out);
    return pack_value_templ<D>(point->dep, out);
}

unsigned char*
pack_func_ipi(const ygor_data_point* prev, ygor_data_point* point, unsigned char* out)
{
    if (prev)
    {
        assert(prev->indep.precise <= point->indep.precise);
        return e::packvarint64(point->indep.precise - prev->indep.precise, out);
    }
    else
    {
        return e::packvarint64(point->indep.precise, out);
    }
}

unsigned char*
pack_func_pi_pi(const ygor_data_point* prev, ygor_data_point* point, unsigned char* out)
{
    out = pack_func_ipi(prev, point, out);
    return pack_value_templ<YGOR_PRECISE_INTEGER>(point->dep, out);
}

unsigned char*
pack_func_pi_hp(const ygor_data_point* prev, ygor_data_point* point, unsigned char* out)
{
    out = pack_func_ipi(prev, point, out);
    return pack_value_templ<YGOR_HALF_PRECISION>(point->dep, out);
}

unsigned char*
pack_func_pi_sp(const ygor_data_point* prev, ygor_data_point* point, unsigned char* out)
{
    out = pack_func_ipi(prev, point, out);
    return pack_value_templ<YGOR_SINGLE_PRECISION>(point->dep, out);
}

unsigned char*
pack_func_pi_dp(const ygor_data_point* prev, ygor_data_point* point, unsigned char* out)
{
    out = pack_func_ipi(prev, point, out);
    return pack_value_templ<YGOR_DOUBLE_PRECISION>(point->dep, out);
}

pack_func_t
pack_func_precise_integer(const ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return pack_func_pi_pi;
        case YGOR_HALF_PRECISION: return pack_func_pi_hp;
        case YGOR_SINGLE_PRECISION: return pack_func_pi_sp;
        case YGOR_DOUBLE_PRECISION: return pack_func_pi_dp;
        default: return NULL;
    }
}

pack_func_t
pack_func_half_precision(const ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return pack_func_templ<YGOR_HALF_PRECISION, YGOR_PRECISE_INTEGER>;
        case YGOR_HALF_PRECISION: return pack_func_templ<YGOR_HALF_PRECISION, YGOR_HALF_PRECISION>;
        case YGOR_SINGLE_PRECISION: return pack_func_templ<YGOR_HALF_PRECISION, YGOR_SINGLE_PRECISION>;
        case YGOR_DOUBLE_PRECISION: return pack_func_templ<YGOR_HALF_PRECISION, YGOR_DOUBLE_PRECISION>;
        default: return NULL;
    }
}

pack_func_t
pack_func_single_precision(const ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return pack_func_templ<YGOR_SINGLE_PRECISION, YGOR_PRECISE_INTEGER>;
        case YGOR_HALF_PRECISION: return pack_func_templ<YGOR_SINGLE_PRECISION, YGOR_HALF_PRECISION>;
        case YGOR_SINGLE_PRECISION: return pack_func_templ<YGOR_SINGLE_PRECISION, YGOR_SINGLE_PRECISION>;
        case YGOR_DOUBLE_PRECISION: return pack_func_templ<YGOR_SINGLE_PRECISION, YGOR_DOUBLE_PRECISION>;
        default: return NULL;
    }
}

pack_func_t
pack_func_double_precision(const ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return pack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_PRECISE_INTEGER>;
        case YGOR_HALF_PRECISION: return pack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_HALF_PRECISION>;
        case YGOR_SINGLE_PRECISION: return pack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_SINGLE_PRECISION>;
        case YGOR_DOUBLE_PRECISION: return pack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_DOUBLE_PRECISION>;
        default: return NULL;
    }
}

pack_func_t
ygor_series_logger :: pack_func(const ygor_series* s)
{
    switch (s->indep_precision)
    {
        case YGOR_PRECISE_INTEGER: return pack_func_precise_integer(s);
        case YGOR_HALF_PRECISION: return pack_func_half_precision(s);
        case YGOR_SINGLE_PRECISION: return pack_func_single_precision(s);
        case YGOR_DOUBLE_PRECISION: return pack_func_double_precision(s);
        default: return NULL;
    }
}

ygor_series_logger :: ygor_series_logger(ygor_data_logger* dl, const ygor_series* s)
    : ydl(dl)
    , ys(s)
    , sindex(ydl->series_index(ys))
    , points_mtx()
    , points(points_A)
    , points_sz(0)
    , io_mtx()
    , sort(sort_func(ys))
    , pack(pack_func(ys))
{
}

ygor_series_logger :: ~ygor_series_logger() throw ()
{
}

int
ygor_series_logger :: record(ygor_data_point* ydp)
{
    points_mtx.lock();
    assert(points_sz < SERIES_BUFFER_SIZE);
    points[points_sz] = *ydp;
    ++points_sz;

    if (points_sz < SERIES_BUFFER_SIZE)
    {
        points_mtx.unlock();
        return 0;
    }

    po6::threads::mutex::hold hold(&io_mtx);
    ygor_data_point* flush = points;
    points = points == points_A ? points_B : points_A;
    points_sz = 0;
    points_mtx.unlock();
    return write(flush, SERIES_BUFFER_SIZE);
}

int
ygor_series_logger :: flush()
{
    po6::threads::mutex::hold holdp(&points_mtx);
    po6::threads::mutex::hold holdi(&io_mtx);
    int ret = write(points, points_sz);
    points_sz = 0;
    return ret;
}

int
ygor_series_logger :: write(ygor_data_point* flush, size_t flush_sz)
{
    std::sort(flush, flush + flush_sz, sort);
    unsigned char buf[sizeof(uint64_t) + VARINT_64_MAX_SIZE + SERIES_BUFFER_SIZE * MAX_POINT_SIZE];
    unsigned char* ptr = buf + sizeof(uint64_t);
    ptr = e::packvarint64(sindex, ptr);

    for (size_t i = 0; i < flush_sz; ++i)
    {
        ptr = pack(i > 0 ? flush + i - 1 : NULL, flush + i, ptr);
    }

    size_t buf_sz = ptr - buf;
    e::pack64be(buf_sz - sizeof(uint64_t), buf);
    po6::threads::mutex::hold hold(&ydl->output_mtx);
    return fwrite(buf, 1, buf_sz, ydl->output) == buf_sz ? 0 : -1;
}

struct ygor_data_reader
{
    ygor_data_reader();
    ~ygor_data_reader() throw ();

    bool init(const char* input);

    std::string input;
    off_t data_offset;
    std::vector<ygor_series> series;
    std::list<std::string> names;

    private:
        ygor_data_reader(const ygor_data_reader&);
        ygor_data_reader& operator = (const ygor_data_reader&);
};

YGOR_API ygor_data_reader*
ygor_data_reader_create(const char* input)
{
    ygor_data_reader* ydr = new ygor_data_reader();

    if (!ydr->init(input))
    {
        delete ydr;
        return NULL;
    }

    return ydr;
}

YGOR_API void
ygor_data_reader_destroy(ygor_data_reader* ydr)
{
    delete ydr;
}

YGOR_API size_t
ygor_data_reader_num_series(ygor_data_reader* ydr)
{
    return ydr->series.size();
}

YGOR_API const ygor_series*
ygor_data_reader_series(ygor_data_reader* ydr, size_t idx)
{
    return &ydr->series[idx];
}

ygor_data_reader :: ygor_data_reader()
    : input()
    , data_offset(0)
    , series()
    , names()
{
}

ygor_data_reader :: ~ygor_data_reader() throw ()
{
}

bool
ygor_data_reader :: init(const char* name)
{
    input = name;
    FILE* fin = fopen(name, "r");

    if (!fin)
    {
        return false;
    }

    e::guard g_fin = e::makeguard(fclose, fin);
    char buffer[512];
    size_t used = 0;

    while (true)
    {
        char* ptr = (char*)memchr(buffer, '\0', used);

        if (!ptr && feof(fin))
        {
            return false;
        }
        else if (!ptr)
        {
            size_t amt = fread(buffer + used, 1, 512 - used, fin);

            if (amt == 0 || ferror(fin))
            {
                return false;
            }

            used += amt;
        }
        else if (ptr == buffer)
        {
            data_offset = ftell(fin) - used + 1;
            break;
        }
        else if (ptr + 4 > buffer + used)
        {
            return false;
        }
        else
        {
            names.push_back(buffer);
            series.push_back(ygor_series());
            series.back().name = names.back().c_str();
            series.back().indep_units = (ygor_units)ptr[1];
            series.back().indep_precision = (ygor_precision)ptr[2];
            series.back().dep_units = (ygor_units)ptr[3];
            series.back().dep_precision = (ygor_precision)ptr[4];

            char* tmp = ptr + 5;
            size_t rem = tmp - buffer;
            memmove(buffer, tmp, used - rem);
            used -= rem;
        }
    }

    return true;
}

struct ygor_data_iterator
{
    ygor_data_iterator();
    virtual ~ygor_data_iterator() throw ();

    virtual ygor_series* series() = 0;
    virtual int valid() = 0;
    virtual void advance() = 0;
    virtual void read(ygor_data_point* ydp) = 0;
    virtual int rewind() = 0;
};

ygor_data_iterator :: ygor_data_iterator()
{
}

ygor_data_iterator :: ~ygor_data_iterator() throw ()
{
}

struct series_iterator : public ygor_data_iterator
{
    static unpack_func_t unpack_func(ygor_series* s);

    series_iterator();
    virtual ~series_iterator() throw ();

    virtual ygor_series* series();
    virtual int valid();
    virtual void advance();
    virtual void read(ygor_data_point* ydp);
    virtual int rewind();

    bool init(ygor_series* s, size_t idx, const char* input, off_t offset);
    bool read(unsigned char* buf, size_t buf_sz);

    ygor_series* m_series;
    size_t m_series_idx;
    FILE* m_input;
    off_t m_offset;

    std::vector<ygor_data_point> m_data;
    size_t m_data_idx;
    bool m_primed;
    bool m_error;
    bool m_eof;
    unpack_func_t m_unpack;

    private:
        series_iterator(const series_iterator&);
        series_iterator& operator = (const series_iterator&);
};

YGOR_API ygor_data_iterator*
ygor_data_iterate(ygor_data_reader* ydr, const char* name)
{
    size_t idx = 0;

    for (idx = 0; idx < ydr->series.size(); ++idx)
    {
        if (strcmp(ydr->series[idx].name, name) == 0)
        {
            break;
        }
    }

    if (idx == ydr->series.size())
    {
        errno = ENOENT;
        return NULL;
    }

    series_iterator* ydi = new series_iterator();

    if (!ydi->init(&ydr->series[idx], idx, ydr->input.c_str(), ydr->data_offset))
    {
        delete ydi;
        return NULL;
    }

    return ydi;
}

YGOR_API void
ygor_data_iterator_destroy(ygor_data_iterator* ydi)
{
    delete ydi;
}

YGOR_API const struct ygor_series*
ygor_data_iterator_series(ygor_data_iterator* ydi)
{
    return ydi->series();
}

YGOR_API int
ygor_data_iterator_valid(ygor_data_iterator* ydi)
{
    return ydi->valid();
}

YGOR_API void
ygor_data_iterator_advance(ygor_data_iterator* ydi)
{
    return ydi->advance();
}

YGOR_API int
ygor_data_iterator_rewind(ygor_data_iterator* ydi)
{
    return ydi->rewind();
}

YGOR_API void
ygor_data_iterator_read(ygor_data_iterator* ydi, ygor_data_point* ydp)
{
    return ydi->read(ydp);
}

template <ygor_precision P>
const unsigned char* unpack_value_templ(const unsigned char* in, const unsigned char* end, ygor_data_value* v);

template <>
const unsigned char*
unpack_value_templ<YGOR_PRECISE_INTEGER>(const unsigned char* in, const unsigned char* end, ygor_data_value* v)
{
    if (!in) return NULL;
    return e::varint64_decode(in, end, &v->precise);
}

template <>
const unsigned char*
unpack_value_templ<YGOR_HALF_PRECISION>(const unsigned char* in, const unsigned char* end, ygor_data_value* v)
{
    if (!in) return NULL;
    if (end - in < 2) return NULL;
    uint16_t h;
    in = e::unpack16be(in, &h);
    v->approximate = halffloat_decompress(h);
    return in;
}

template <>
const unsigned char*
unpack_value_templ<YGOR_SINGLE_PRECISION>(const unsigned char* in, const unsigned char* end, ygor_data_value* v)
{
    if (!in) return NULL;
    if (end - in < 4) return NULL;
    float x;
    in = e::unpackfloatbe(in, &x);
    v->approximate = x;
    return in;
}

template <>
const unsigned char*
unpack_value_templ<YGOR_DOUBLE_PRECISION>(const unsigned char* in, const unsigned char* end, ygor_data_value* v)
{
    if (!in) return NULL;
    if (end - in < 8) return NULL;
    return e::unpackdoublebe(in, &v->approximate);
}

template <ygor_precision I, ygor_precision D>
const unsigned char*
unpack_func_templ(const unsigned char* in, const unsigned char* end, const ygor_data_point*, ygor_data_point* point)
{
    in = unpack_value_templ<I>(in, end, &point->indep);
    return unpack_value_templ<D>(in, end, &point->dep);
}

const unsigned char*
unpack_func_ipi(const unsigned char* in, const unsigned char* end, const ygor_data_point* prev, ygor_data_point* point)
{
    if (!in) return NULL;
    in = e::varint64_decode(in, end, &point->indep.precise);

    if (prev)
    {
        point->indep.precise += prev->indep.precise;
    }

    return in;
}

const unsigned char*
unpack_func_pi_pi(const unsigned char* in, const unsigned char* end, const ygor_data_point* prev, ygor_data_point* point)
{
    in = unpack_func_ipi(in, end, prev, point);
    return unpack_value_templ<YGOR_PRECISE_INTEGER>(in, end, &point->dep);
}

const unsigned char*
unpack_func_pi_hp(const unsigned char* in, const unsigned char* end, const ygor_data_point* prev, ygor_data_point* point)
{
    in = unpack_func_ipi(in, end, prev, point);
    return unpack_value_templ<YGOR_HALF_PRECISION>(in, end, &point->dep);
}

const unsigned char*
unpack_func_pi_sp(const unsigned char* in, const unsigned char* end, const ygor_data_point* prev, ygor_data_point* point)
{
    in = unpack_func_ipi(in, end, prev, point);
    return unpack_value_templ<YGOR_SINGLE_PRECISION>(in, end, &point->dep);
}

const unsigned char*
unpack_func_pi_dp(const unsigned char* in, const unsigned char* end, const ygor_data_point* prev, ygor_data_point* point)
{
    in = unpack_func_ipi(in, end, prev, point);
    return unpack_value_templ<YGOR_DOUBLE_PRECISION>(in, end, &point->dep);
}

unpack_func_t
unpack_func_precise_integer(ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return unpack_func_pi_pi;
        case YGOR_HALF_PRECISION: return unpack_func_pi_hp;
        case YGOR_SINGLE_PRECISION: return unpack_func_pi_sp;
        case YGOR_DOUBLE_PRECISION: return unpack_func_pi_dp;
        default: return NULL;
    }
}

unpack_func_t
unpack_func_half_precision(ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return unpack_func_templ<YGOR_HALF_PRECISION, YGOR_PRECISE_INTEGER>;
        case YGOR_HALF_PRECISION: return unpack_func_templ<YGOR_HALF_PRECISION, YGOR_HALF_PRECISION>;
        case YGOR_SINGLE_PRECISION: return unpack_func_templ<YGOR_HALF_PRECISION, YGOR_SINGLE_PRECISION>;
        case YGOR_DOUBLE_PRECISION: return unpack_func_templ<YGOR_HALF_PRECISION, YGOR_DOUBLE_PRECISION>;
        default: return NULL;
    }
}

unpack_func_t
unpack_func_single_precision(ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return unpack_func_templ<YGOR_SINGLE_PRECISION, YGOR_PRECISE_INTEGER>;
        case YGOR_HALF_PRECISION: return unpack_func_templ<YGOR_SINGLE_PRECISION, YGOR_HALF_PRECISION>;
        case YGOR_SINGLE_PRECISION: return unpack_func_templ<YGOR_SINGLE_PRECISION, YGOR_SINGLE_PRECISION>;
        case YGOR_DOUBLE_PRECISION: return unpack_func_templ<YGOR_SINGLE_PRECISION, YGOR_DOUBLE_PRECISION>;
        default: return NULL;
    }
}

unpack_func_t
unpack_func_double_precision(ygor_series* s)
{
    switch (s->dep_precision)
    {
        case YGOR_PRECISE_INTEGER: return unpack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_PRECISE_INTEGER>;
        case YGOR_HALF_PRECISION: return unpack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_HALF_PRECISION>;
        case YGOR_SINGLE_PRECISION: return unpack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_SINGLE_PRECISION>;
        case YGOR_DOUBLE_PRECISION: return unpack_func_templ<YGOR_DOUBLE_PRECISION, YGOR_DOUBLE_PRECISION>;
        default: return NULL;
    }
}

unpack_func_t
series_iterator :: unpack_func(ygor_series* s)
{
    switch (s->indep_precision)
    {
        case YGOR_PRECISE_INTEGER: return unpack_func_precise_integer(s);
        case YGOR_HALF_PRECISION: return unpack_func_half_precision(s);
        case YGOR_SINGLE_PRECISION: return unpack_func_single_precision(s);
        case YGOR_DOUBLE_PRECISION: return unpack_func_double_precision(s);
        default: return NULL;
    }
}

series_iterator :: series_iterator()
    : m_series(NULL)
    , m_series_idx(0)
    , m_input(NULL)
    , m_offset(0)
    , m_data()
    , m_data_idx(0)
    , m_primed(false)
    , m_error(false)
    , m_eof(false)
    , m_unpack()
{
}

series_iterator :: ~series_iterator() throw ()
{
    if (m_input)
    {
        fclose(m_input);
    }
}

ygor_series*
series_iterator :: series()
{
    return m_series;
}

int
series_iterator :: valid()
{
    if (m_error)
    {
        return -1;
    }

    if (m_primed)
    {
        return 1;
    }

    if (m_data_idx < m_data.size())
    {
        m_primed = true;
        return 1;
    }

    while (true)
    {
        m_data.clear();
        m_data_idx = 0;
        unsigned char buf[VARINT_64_MAX_SIZE + SERIES_BUFFER_SIZE * MAX_POINT_SIZE];

        if (!read(buf, sizeof(uint64_t)))
        {
            return m_eof ? 0 : -1;
        }

        uint64_t block_sz;
        e::unpack64be(buf, &block_sz);

        if (!read(buf, block_sz))
        {
            m_error = true;
            return -1;
        }

        const unsigned char* ptr = buf;
        const unsigned char* end = ptr + block_sz;
        uint64_t series;
        ptr = e::varint64_decode(ptr, end, &series);

        if (!ptr)
        {
            m_error = true;
            return -1;
        }

        if (series != m_series_idx)
        {
            continue;
        }

        while (ptr < end)
        {
            ygor_data_point* prev = !m_data.empty()
                                  ? &m_data.back() : NULL;
            ygor_data_point p;
            const unsigned char* tmp = m_unpack(ptr, end, prev, &p);

            if (!tmp)
            {
                m_error = true;
                return -1;
            }

            p.series = m_series;
            m_data.push_back(p);
            ptr = tmp;
        }

        assert(ptr == end);

        if (!m_data.empty())
        {
            m_primed = true;
            return 1;
        }
    }
}

void
series_iterator :: advance()
{
    assert(valid() > 0);
    ++m_data_idx;
    m_primed = false;
}

void
series_iterator :: read(ygor_data_point* ydp)
{
    assert(valid() > 0);
    *ydp = m_data[m_data_idx];
}

int
series_iterator :: rewind()
{
    m_data.clear();
    m_data_idx = 0;
    m_primed = false;
    m_error = false;
    m_eof = false;
    return fseek(m_input, m_offset, SEEK_SET) >= 0 ? 0 : -1;
}

bool
series_iterator :: init(ygor_series* s, size_t idx, const char* name, off_t offset)
{
    m_series = s;
    m_series_idx = idx;
    m_offset = offset;
    m_unpack = unpack_func(m_series);
    m_input = fopen(name, "r");

    if (!m_input || fseek(m_input, offset, SEEK_SET) < 0)
    {
        return false;
    }

    return true;
}

bool
series_iterator :: read(unsigned char* buf, size_t buf_sz)
{
    if (m_error)
    {
        return false;
    }

    if (fread(buf, 1, buf_sz, m_input) != buf_sz)
    {
        m_error = ferror(m_input) != 0;
        m_eof = !m_error && feof(m_input) != 0;
        return false;
    }

    return true;
}

YGOR_API int
ygor_data_iterator_sample(ygor_data_iterator* ydi,
                          ygor_data_point* ydp, size_t ydp_sz,
                          size_t* k, size_t* n)
{
    if (ydp_sz <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    guacamole g;
    guacamole_seed(&g, (intptr_t)ydi);
    size_t elem = 0;
    int status = 0;

    while ((status = ygor_data_iterator_valid(ydi)) > 0)
    {
        ygor_data_point p;
        ygor_data_iterator_read(ydi, &p);
        ygor_data_iterator_advance(ydi);

        if (elem < ydp_sz)
        {
            ydp[elem] = p;
        }
        else
        {
            size_t idx = guacamole_double(&g) * elem;

            if (idx < ydp_sz)
            {
                ydp[idx] = p;
            }
        }

        ++elem;
    }

    if (status < 0)
    {
        return -1;
    }

    *n = elem;
    *k = std::min(ydp_sz, *n);
    return 0;
}

struct convert
{
    ygor_units from;
    ygor_units to;
    double rate;
};

static const convert conversion_rates[] = {{YGOR_UNIT_S, YGOR_UNIT_MS, 1000},
                                           {YGOR_UNIT_S, YGOR_UNIT_US, 1000000},
                                           {YGOR_UNIT_MS, YGOR_UNIT_US, 1000},
                                           {YGOR_UNIT_US, YGOR_UNIT_S, 0.000001},
                                           {YGOR_UNIT_US, YGOR_UNIT_MS, 0.001},
                                           {YGOR_UNIT_MS, YGOR_UNIT_S, 0.001},
                                           {YGOR_UNIT_BYTES, YGOR_UNIT_KBYTES, 0.001},
                                           {YGOR_UNIT_BYTES, YGOR_UNIT_MBYTES, 0.000001},
                                           {YGOR_UNIT_BYTES, YGOR_UNIT_GBYTES, 0.000000001},
                                           {YGOR_UNIT_KBYTES, YGOR_UNIT_MBYTES, 0.001},
                                           {YGOR_UNIT_KBYTES, YGOR_UNIT_GBYTES, 0.000001},
                                           {YGOR_UNIT_MBYTES, YGOR_UNIT_GBYTES, 0.001},
                                           {YGOR_UNIT_GBYTES, YGOR_UNIT_MBYTES, 1000},
                                           {YGOR_UNIT_GBYTES, YGOR_UNIT_KBYTES, 1000000},
                                           {YGOR_UNIT_GBYTES, YGOR_UNIT_BYTES, 1000000000},
                                           {YGOR_UNIT_MBYTES, YGOR_UNIT_KBYTES, 1000},
                                           {YGOR_UNIT_MBYTES, YGOR_UNIT_BYTES, 1000000},
                                           {YGOR_UNIT_KBYTES, YGOR_UNIT_BYTES, 1000},
                                           {(ygor_units)0, (ygor_units)0, 0}};

YGOR_API int
ygor_units_compatible(ygor_units from, ygor_units to)
{
    const convert* c = conversion_rates;

    while (c->from && c->to)
    {
        if (c->from == from && c->to == to)
        {
            return true;
        }

        ++c;
    }

    return from == to;
}

YGOR_API double
ygor_units_conversion_ratio(ygor_units from, ygor_units to)
{
    assert(ygor_units_compatible(from, to));
    const convert* c = conversion_rates;

    while (c->from && c->to)
    {
        if (c->from == from && c->to == to)
        {
            return c->rate;
        }

        ++c;
    }

    assert(from == to);
    return 1.0;
}

struct conversion_iterator : public ygor_data_iterator
{
    conversion_iterator();
    virtual ~conversion_iterator() throw ();

    virtual ygor_series* series();
    virtual int valid();
    virtual void advance();
    virtual void read(ygor_data_point* ydp);
    virtual int rewind();

    ygor_data_iterator* m_it;
    ygor_series m_series;
    double m_indep_scale;
    double m_dep_scale;

    private:
        conversion_iterator(const conversion_iterator&);
        conversion_iterator& operator = (const conversion_iterator&);
};

YGOR_API struct ygor_data_iterator*
ygor_data_convert_units(ygor_data_iterator* ydi,
                        ygor_units new_indep_units,
                        ygor_units new_dep_units)
{
    if (!ygor_units_compatible(ydi->series()->indep_units, new_indep_units) ||
        !ygor_units_compatible(ydi->series()->dep_units, new_dep_units))
    {
        return NULL;
    }

    conversion_iterator* ci = new conversion_iterator();
    ci->m_it = ydi;
    ci->m_series = *ydi->series();

    if (ydi->series()->indep_units != new_indep_units)
    {
        ci->m_series.indep_precision = approximate_precision(ci->m_series.indep_precision);
    }

    ci->m_series.indep_units = new_indep_units;

    if (ydi->series()->dep_units != new_dep_units)
    {
        ci->m_series.dep_precision = approximate_precision(ci->m_series.dep_precision);
    }

    ci->m_series.dep_units = new_dep_units;
    ci->m_indep_scale = ygor_units_conversion_ratio(ydi->series()->indep_units, new_indep_units);
    ci->m_dep_scale = ygor_units_conversion_ratio(ydi->series()->dep_units, new_dep_units);
    return ci;
}

conversion_iterator :: conversion_iterator()
    : m_it(NULL)
    , m_series()
    , m_indep_scale(1.0)
    , m_dep_scale(1.0)
{
}

conversion_iterator :: ~conversion_iterator() throw ()
{
    delete m_it;
}

ygor_series*
conversion_iterator :: series()
{
    return &m_series;
}

int
conversion_iterator :: valid()
{
    return m_it->valid();
}

void
conversion_iterator :: advance()
{
    return m_it->advance();
}

void
conversion_iterator :: read(ygor_data_point* ydp)
{
    m_it->read(ydp);

    if (ygor_is_precise(m_series.indep_precision))
    {
        assert(m_indep_scale < 1.0001 && m_indep_scale > 0.9999);
    }
    else if (ygor_is_precise(ydp->series->indep_precision))
    {
        ydp->indep.approximate = ydp->indep.precise * m_indep_scale;
    }
    else
    {
        ydp->indep.approximate *= m_indep_scale;
    }

    if (ygor_is_precise(m_series.dep_precision))
    {
        assert(m_dep_scale < 1.0001 && m_dep_scale > 0.9999);
    }
    else if (ygor_is_precise(ydp->series->dep_precision))
    {
        ydp->dep.approximate = ydp->dep.precise * m_dep_scale;
    }
    else
    {
        ydp->dep.approximate *= m_dep_scale;
    }

    ydp->series = &m_series;
}

int
conversion_iterator :: rewind()
{
    return m_it->rewind();
}

bool
compare_by_precise_indep(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.indep.precise < rhs.indep.precise;
}

bool
compare_by_approx_indep(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.indep.approximate < rhs.indep.approximate;
}

typedef bool (*cmp_indep_t)(const ygor_data_point&, const ygor_data_point&);

cmp_indep_t
compare_by_indep(ygor_precision indep_precision)
{
    switch (indep_precision)
    {
        case YGOR_PRECISE_INTEGER:
            return compare_by_precise_indep;
        case YGOR_HALF_PRECISION:
        case YGOR_SINGLE_PRECISION:
        case YGOR_DOUBLE_PRECISION:
            return compare_by_approx_indep;
        default:
            abort();
    }
}

bool
compare_by_precise_dep(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.dep.precise < rhs.dep.precise;
}

bool
compare_by_approx_dep(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.dep.approximate < rhs.dep.approximate;
}

typedef bool (*cmp_dep_t)(const ygor_data_point&, const ygor_data_point&);

cmp_dep_t
compare_by_dep(ygor_precision dep_precision)
{
    switch (dep_precision)
    {
        case YGOR_PRECISE_INTEGER:
            return compare_by_precise_dep;
        case YGOR_HALF_PRECISION:
        case YGOR_SINGLE_PRECISION:
        case YGOR_DOUBLE_PRECISION:
            return compare_by_approx_dep;
        default:
            abort();
    }
}

double
extract_dep_double(const ygor_data_point& ydp)
{
    double value = ydp.dep.precise;

    if (!ygor_is_precise(ydp.series->dep_precision))
    {
        value = ydp.dep.approximate;
    }

    return value;
}

YGOR_API int
ygor_cdf(ygor_data_iterator* ydi, uint64_t step_value,
         ygor_data_point** data, uint64_t* data_sz)
{
    *data = NULL;
    *data_sz = 0;
    std::vector<ygor_data_point> points;
    points.push_back(ygor_data_point());
    points[0].series = ygor_data_iterator_series(ydi);
    points[0].indep.precise = 0;
    points[0].dep.precise = 0;
    uint64_t num_points = 0;
    int status = 0;

    while ((status = ygor_data_iterator_valid(ydi)) > 0)
    {
        ygor_data_point ydp;
        ygor_data_iterator_read(ydi, &ydp);
        ygor_data_iterator_advance(ydi);
        double value = extract_dep_double(ydp);
        size_t idx = 0;

        while (points[idx].indep.precise < value)
        {
            if (idx + 1 == points.size())
            {
                ygor_data_point next;
                next.series = points[idx].series;
                next.indep.precise = points[idx].indep.precise + step_value;
                next.dep.precise = 0;
                points.push_back(next);
            }

            ++idx;
        }

        points[idx].dep.precise += 1;
        ++num_points;
    }

    if (status < 0)
    {
        return -1;
    }

    if (num_points == 0)
    {
        *data = NULL;
        *data_sz = 0;
        return 0;
    }

    uint64_t sum = 0;
    const size_t sz = sizeof(ygor_data_point) * points.size();
    *data = (ygor_data_point*)malloc(sz);
    *data_sz = points.size();

    for (size_t i = 0; i < points.size(); ++i)
    {
        (*data)[i].indep = points[i].indep;
        sum += points[i].dep.precise;
        (*data)[i].dep.approximate = 100. * (double)sum
                                          / (double)num_points;
    }

    return 0;
}

#define PERCENTILE_BUFFER_SZ (1ULL << 10)

YGOR_API int
ygor_percentile(ygor_data_iterator* ydi, double percentile, double* value)
{
    if (percentile <= 0 || percentile > 1)
    {
        errno = EINVAL;
        return -1;
    }

    std::vector<ygor_data_point> sampled(PERCENTILE_BUFFER_SZ);
    size_t k = 0;
    size_t n = 0;

    if (ygor_data_iterator_sample(ydi, &sampled[0], PERCENTILE_BUFFER_SZ, &k, &n) < 0)
    {
        return -1;
    }

    assert(k <= n);
    cmp_indep_t cmp = compare_by_dep(ygor_data_iterator_series(ydi)->dep_precision);
    std::sort(&sampled[0], &sampled[0] + k, cmp);

    if (k == n)
    {
        if (n == 0)
        {
            *value = NAN;
            return 0;
        }

        size_t which = (n - 1) * percentile;
        *value = extract_dep_double(sampled[which]);
        return 0;
    }

    const size_t window = PERCENTILE_BUFFER_SZ * .25 * k / n;
    const size_t center = k * percentile;
    size_t lower_cutoff_idx = center > window ? center - window : 0;
    size_t upper_cutoff_idx = std::min(center + 3 * window, k);
    double lower_cutoff = -INFINITY;
    double upper_cutoff = INFINITY;

    if (lower_cutoff_idx > 0 && lower_cutoff_idx <= upper_cutoff_idx)
    {
        assert(lower_cutoff_idx < k);
        lower_cutoff = extract_dep_double(sampled[lower_cutoff_idx]);
    }

    if (upper_cutoff_idx < k && lower_cutoff_idx < upper_cutoff_idx)
    {
        upper_cutoff = extract_dep_double(sampled[upper_cutoff_idx]);
    }
    else if (upper_cutoff_idx + 1 < k && lower_cutoff_idx == upper_cutoff_idx)
    {
        upper_cutoff = extract_dep_double(sampled[upper_cutoff_idx + 1]);
    }

    std::vector<double> values(PERCENTILE_BUFFER_SZ);

    for (size_t iteration = 0; ; ++iteration)
    {
        if (ygor_data_iterator_rewind(ydi) < 0)
        {
            return -1;
        }

        assert(lower_cutoff <= upper_cutoff);
        uint64_t lower_count = 0;
        uint64_t upper_count = 0;
        size_t idx = 0;
        int status = 0;

        while ((status = ygor_data_iterator_valid(ydi)) > 0)
        {
            if (idx >= values.size())
            {
                std::sort(values.begin(), values.end());

                if (values[0] >= values[idx / 4])
                {
                    values.resize(values.size() * 2);
                }
                else
                {
                    const double* start = &values[0];
                    const double* limit = start + idx;
                    const double* first = std::upper_bound(start, limit, lower_cutoff);
                    assert(first < limit);
                    const double* cut = first + (limit - first) / 2;
                    assert(cut < limit);
                    upper_cutoff = *cut;
                    upper_count += limit - cut;
                    idx = cut - start;
                }
            }

            assert(idx < values.size());
            ygor_data_point p;
            ygor_data_iterator_read(ydi, &p);
            ygor_data_iterator_advance(ydi);
            double v = extract_dep_double(p);

            if (v < lower_cutoff)
            {
                ++lower_count;
            }
            // compare to lower_cutoff to correct for when 
            // lower_cutoff == upper_cutoff
            else if (v > lower_cutoff && v >= upper_cutoff)
            {
                ++upper_count;
            }
            else
            {
                assert(idx < values.size());
                values[idx] = v;
                ++idx;
            }
        }

        if (status < 0)
        {
            return -1;
        }

        if (idx + lower_count + upper_count != n)
        {
            abort(); // XXX
        }

        const size_t which = (n - 1) * percentile;
        const ssize_t adj = window * 2;
        std::sort(&values[0], &values[0] + idx);

        if (which < lower_count)
        {
            if (idx == 0)
            {
                values[0] = upper_cutoff;
                idx = 1;
            }

            lower_cutoff = -INFINITY;
            upper_cutoff = *std::min_element(&values[0], &values[0] + idx);
            std::vector<ygor_data_point>::iterator it = sampled.begin();

            while (it + adj < sampled.end() && extract_dep_double(*(it + adj)) < upper_cutoff)
            {
                lower_cutoff = extract_dep_double(*it);
                ++it;
            }
        }
        else if (idx == 0 || which - lower_count >= idx)
        {
            lower_cutoff = upper_cutoff;
            upper_cutoff = INFINITY;
            std::vector<ygor_data_point>::reverse_iterator it = sampled.rbegin();

            while (it + adj < sampled.rend() && extract_dep_double(*(it + adj)) > lower_cutoff)
            {
                upper_cutoff = extract_dep_double(*it);
                ++it;
            }
        }
        else
        {
            *value = values[which - lower_count];
            return 0;
        }
    }
}

YGOR_API int
ygor_timeseries(ygor_data_iterator* ydi, uint64_t step_value,
                ygor_data_point** data, uint64_t* data_sz)
{
    std::vector<ygor_data_point> points;
    int status = 0;

    while ((status = ygor_data_iterator_valid(ydi)) > 0)
    {
        ygor_data_point ydp;
        ygor_data_iterator_read(ydi, &ydp);
        ygor_data_iterator_advance(ydi);
        uint64_t value = ydp.indep.precise;

        if (!ygor_is_precise(ydp.series->indep_precision))
        {
            value = ydp.indep.approximate;
        }

        std::vector<ygor_data_point>::reverse_iterator rit = points.rbegin();

        while (rit != points.rend())
        {
            if (rit->indep.precise <= value && value < rit->indep.precise + step_value)
            {
                break;
            }

            ++rit;
        }

        if (rit == points.rend())
        {
            ydp.indep.precise = (uint64_t)(value / step_value) * step_value;
            ydp.dep.precise = 1;
            points.push_back(ydp);
            assert(ydp.indep.precise <= value && value < ydp.indep.precise + step_value);
        }
        else
        {
            rit->dep.precise += 1;
        }
    }

    if (status < 0)
    {
        return -1;
    }

    if (points.empty())
    {
        *data = NULL;
        *data_sz = 0;
        return 0;
    }

    std::sort(points.begin(), points.end(), compare_by_precise_indep);
    const size_t num_points = (points.back().indep.precise
                            - points.front().indep.precise) / step_value + 1;
    assert(num_points >= points.size());
    const size_t sz = sizeof(ygor_data_point) * num_points;
    *data = (ygor_data_point*)malloc(sz);
    *data_sz = num_points;
    const uint64_t base = points.front().indep.precise;

    for (size_t i = 0; i < num_points; ++i)
    {
        (*data)[i].indep.precise = base + i * step_value;
        (*data)[i].dep.precise = 0;
    }

    for (size_t i = 0; i < points.size(); ++i)
    {
        size_t idx = (points[i].indep.precise - base) / step_value;
        (*data)[idx].dep = points[i].dep;
    }

    return 0;
}
