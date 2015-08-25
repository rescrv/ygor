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

// C
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

// POSIX
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

// STL
#include <algorithm>
#include <vector>

// po6
#include <po6/threads/mutex.h>
#include <po6/time.h>

// e
#include <e/varint.h>

// Ygor
#include "ygor.h"
#include "ygor-internal.h"

namespace
{

size_t
roundup(size_t x, size_t y)
{
    return ((x + y - 1) / y) * y;
}

} // namespace

bool
operator < (const struct ygor_data_record& lhs,
            const struct ygor_data_record& rhs)
{
    return lhs.when < rhs.when;
}

bool
operator < (const struct ygor_data_point& lhs,
            const struct ygor_data_point& rhs)
{
    return lhs.x < rhs.x;
}

struct ygor_data_logger
{
    ygor_data_logger();
    ~ygor_data_logger() throw ();
    int open(const char* output, uint64_t scale_when, uint64_t scale_data);
    int flush();
    int close();
    int record(ygor_data_record* dr);

    private:
        ygor_data_logger(const ygor_data_logger&);
        ygor_data_logger& operator = (const ygor_data_logger&);

        bool do_write(std::vector<ygor_data_record>* drs);
        bool do_write(ygor_data_record* start, ygor_data_record* limit);

        po6::threads::mutex m_data_mtx;
        std::vector<ygor_data_record> m_data;
        po6::threads::mutex m_io_mtx;
        FILE* m_out;
        uint64_t m_scale_when;
        uint64_t m_scale_data;
        pid_t m_child;
};

ygor_data_logger :: ygor_data_logger()
    : m_data_mtx()
    , m_data()
    , m_io_mtx()
    , m_out(NULL)
    , m_scale_when(0)
    , m_scale_data(0)
    , m_child(-1)
{
}

ygor_data_logger :: ~ygor_data_logger() throw ()
{
    po6::threads::mutex::hold holdd(&m_data_mtx);
    po6::threads::mutex::hold holdi(&m_io_mtx);

    if (m_out)
    {
        fflush(m_out);
        fclose(m_out);
    }

    if (m_child > 0)
    {
        // nothing we can do from here on error
        int status = 0;
        waitpid(m_child, &status, 0);
    }
}

int
ygor_data_logger :: open(const char* output,
                         uint64_t scale_when,
                         uint64_t scale_data)
{
    using ::close;
    size_t output_sz = strlen(output);
    bool is_bz2 = output_sz >= 4 && strcmp(output + output_sz - 4, ".bz2") == 0;

    po6::threads::mutex::hold hold(&m_io_mtx);

    if (is_bz2)
    {
        int outfd = ::open(output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

        if (outfd < 0)
        {
            return -1;
        }

        int pipefd[2];

        if (pipe(pipefd) < 0)
        {
            close(outfd);
            return -1;
        }

        pid_t child = fork();

        if (child < 0)
        {
            close(outfd);
            close(pipefd[0]);
            close(pipefd[1]);
            return -1;
        }
        else if (child == 0)
        {
            close(pipefd[1]);

            if (dup2(pipefd[0], STDIN_FILENO) < 0 ||
                dup2(outfd, STDOUT_FILENO) < 0)
            {
                perror("dup2");
                exit(1);
            }

            if (pipefd[0] != STDIN_FILENO)
            {
                close(pipefd[0]);
            }

            if (outfd != STDOUT_FILENO)
            {
                close(outfd);
            }

            const char* args[3];
            args[0] = "bzip2";
            args[1] = "--best";
            args[2] = NULL;

            if (execvp(args[0], const_cast<char* const*>(args)) < 0)
            {
                perror("exec");
                exit(1);
            }

            abort();
        }
        else
        {
            close(pipefd[0]);
            close(outfd);
            m_out = fdopen(pipefd[1], "w");
            m_child = child;
        }
    }
    else
    {
        m_out = fopen(output, "w");
    }

    if (m_out == NULL)
    {
        return -1;
    }

    char buf[20];
    char* ptr = buf;
    ptr = e::varint64_encode(ptr, scale_when);
    ptr = e::varint64_encode(ptr, scale_data);

    if (fwrite(buf, ptr - buf, 1, m_out) != 1)
    {
        return -1;
    }

    m_scale_when = scale_when;
    m_scale_data = scale_data;
    return 0;
}

int
ygor_data_logger :: flush()
{
    po6::threads::mutex::hold holdd(&m_data_mtx);
    po6::threads::mutex::hold holdi(&m_io_mtx);
    assert(m_out);
    int ret = 0;

    if (!do_write(&m_data))
    {
        ret = -1;
    }

    if (fflush(m_out) != 0)
    {
        ret = -1;
    }

    return ret;
}

int
ygor_data_logger :: close()
{
    po6::threads::mutex::hold hold(&m_io_mtx);
    assert(m_out);
    int ret = fclose(m_out) == 0 ? 0 : -1;
    m_out = NULL;
    return ret;
}

int
ygor_data_logger :: record(ygor_data_record* dr)
{
    dr->when = floor(double(dr->when) / double(m_scale_when) + 0.5);

    if (dr->series < 0xffff0000U)
    {
        dr->data = floor(double(dr->data) / double(m_scale_data) + 0.5);
    }

    m_data_mtx.lock();
    m_data.push_back(*dr);

    if (m_data.size() > 65536)
    {
        std::vector<ygor_data_record> data;
        m_data.swap(data);
        po6::threads::mutex::hold hold(&m_io_mtx);
        m_data_mtx.unlock();
        return do_write(&data) ? 0 : -1;
    }
    else
    {
        m_data_mtx.unlock();
        return true;
    }
}

bool
ygor_data_logger :: do_write(std::vector<ygor_data_record>* drs)
{
    std::sort(drs->begin(), drs->end(), compare_by_series_then_when);

    if (drs->empty())
    {
        return true;
    }

    ygor_data_record* ptr = &(*drs)[0];
    ygor_data_record* const end = ptr + drs->size();

    while (ptr < end)
    {
        // end of contiguous range with the same series
        ygor_data_record* eor = ptr;

        while (eor < end && ptr->series == eor->series)
        {
            ++eor;
        }

        if (!do_write(ptr, eor))
        {
            return false;
        }

        ptr = eor;
    }

    return true;
}

bool
ygor_data_logger :: do_write(ygor_data_record* start,
                             ygor_data_record* limit)
{
    const size_t buf_sz = 2 * 10;
    char buf[buf_sz];
    char* ptr = buf;
    ptr = e::varint64_encode(ptr, limit - start);
    ptr = e::varint64_encode(ptr, start->series);

    if (fwrite(buf, ptr - buf, 1, m_out) != 1)
    {
        return false;
    }

    uint64_t prev = 0;

    for (ssize_t i = 0; i < limit - start; ++i)
    {
        assert(start[i].when >= prev);
        ptr = buf;
        ptr = e::varint64_encode(ptr, start[i].when - prev);
        ptr = e::varint64_encode(ptr, start[i].data);
        prev = start[i].when;

        if (fwrite(buf, ptr - buf, 1, m_out) != 1)
        {
            return false;
        }
    }

    return true;
}

class ygor_data_iterator
{
    public:
        ygor_data_iterator();
        ~ygor_data_iterator() throw ();

    public:
        int open(const char* input);
        uint64_t when_scale() { return m_when_scale; }
        uint64_t data_scale() { return m_data_scale; }
        int valid();
        void advance();
        void read(ygor_data_record* dr);

    private:
        ygor_data_iterator(const ygor_data_iterator&);
        ygor_data_iterator& operator = (const ygor_data_iterator&);

    private:
        bool read_number(uint64_t* num);

    private:
        FILE* m_in;
        pid_t m_child;
        uint32_t m_series;
        std::vector<ygor_data_record> m_data;
        size_t m_data_idx;
        uint64_t m_when_scale;
        uint64_t m_data_scale;
        bool m_primed;
        bool m_error;
        bool m_eof;
};

ygor_data_iterator :: ygor_data_iterator()
    : m_in(NULL)
    , m_child(-1)
    , m_series(0)
    , m_data()
    , m_data_idx(0)
    , m_when_scale(1)
    , m_data_scale(1)
    , m_primed(false)
    , m_error(false)
    , m_eof(false)
{
}

ygor_data_iterator :: ~ygor_data_iterator() throw ()
{
    if (m_in)
    {
        fclose(m_in);
    }

    if (m_child > 0)
    {
        // nothing we can do from here on error
        int status = 0;
        waitpid(m_child, &status, 0);
    }
}

int
ygor_data_iterator :: open(const char* input)
{
    using ::close;

    // read the series
    const char* atsym = strchr(input, '@');
    m_series = 0;

    if (atsym)
    {
        std::string series_str(input, atsym);
        char* end = NULL;
        uint32_t series = strtoul(series_str.c_str(), &end, 0);

        if (end && *end == '\0')
        {
            m_series = series;
        }
        else
        {
            errno = EINVAL;
            return -1;
        }

        input = atsym + 1;
    }

    // check if we're reading a bz2 file
    size_t input_sz = strlen(input);
    bool is_bz2 = input_sz >= 4 && strcmp(input + input_sz - 4, ".bz2") == 0;

    if (is_bz2)
    {
        int infd = ::open(input, O_RDONLY);

        if (infd < 0)
        {
            return -1;
        }

        int pipefd[2];

        if (pipe(pipefd) < 0)
        {
            close(infd);
            return -1;
        }

        pid_t child = fork();

        if (child < 0)
        {
            close(infd);
            close(pipefd[0]);
            close(pipefd[1]);
            return -1;
        }
        else if (child == 0)
        {
            close(pipefd[0]);

            if (dup2(infd, STDIN_FILENO) < 0 ||
                dup2(pipefd[1], STDOUT_FILENO) < 0)
            {
                perror("dup2");
                exit(1);
            }

            if (pipefd[1] != STDIN_FILENO)
            {
                close(pipefd[1]);
            }

            if (infd != STDOUT_FILENO)
            {
                close(infd);
            }

            const char* args[2];
            args[0] = "bunzip2";
            args[1] = NULL;

            if (execvp(args[0], const_cast<char* const*>(args)) < 0)
            {
                perror("exec");
                exit(1);
            }

            abort();
        }
        else
        {
            close(pipefd[1]);
            close(infd);
            m_in = fdopen(pipefd[0], "r");
            m_child = child;
        }
    }
    else
    {
        m_in = fopen(input, "r");
    }

    if (m_in == NULL)
    {
        m_error = true;
        return -1;
    }

    if (!read_number(&m_when_scale) ||
        !read_number(&m_data_scale))
    {
        m_error = true;
        return -1;
    }

    return 0;
}

int
ygor_data_iterator :: valid()
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
        uint64_t num_data_points;
        uint64_t series;

        if (!read_number(&num_data_points) ||
            !read_number(&series))
        {
            return m_eof ? 0 : -1;
        }

        if (m_data.capacity() < num_data_points &&
            num_data_points > 0)
        {
            m_data.reserve(num_data_points);
        }

        // overflow on cast?
        if ((series & UINT32_MAX) != series)
        {
            m_error = true;
            return -1;
        }

        uint64_t prev = 0;

        for (size_t i = 0; i < num_data_points; ++i)
        {
            uint64_t when;
            uint64_t data;

            if (!read_number(&when) ||
                !read_number(&data))
            {
                return m_eof ? 0 : -1;
            }

            when += prev;

            if ((m_series == 0 && series < 0xffff0000U) ||
                m_series == UINT32_MAX ||
                series == m_series)
            {
                ygor_data_record dr;
                dr.series = series;
                dr.when = when * m_when_scale;
                dr.data = data;

                if (dr.series < 0xffff0000U)
                {
                    dr.data *= m_data_scale;
                }

                m_data.push_back(dr);
            }

            prev = when;
        }

        if (m_data_idx < m_data.size())
        {
            m_primed = true;
            return 1;
        }
    }
}

void
ygor_data_iterator :: advance()
{
    assert(valid());
    ++m_data_idx;
    m_primed = false;
}

void
ygor_data_iterator :: read(ygor_data_record* data)
{
    assert(valid());
    *data = m_data[m_data_idx];
}

bool
ygor_data_iterator :: read_number(uint64_t* num)
{
    if (m_error)
    {
        return false;
    }

    char buf[10];
    memset(buf, 0, sizeof(buf));
    int c = 0;

    for (int i = 0; i < 10; ++i)
    {
        c = fgetc(m_in);

        if (c == EOF && feof(m_in) != 0 && i == 0)
        {
            m_eof = true;
            return false;
        }
        if (c == EOF)
        {
            m_error = true;
            return false;
        }

        buf[i] = c;

        if (!(c & 0x80))
        {
            const char* ptr = e::varint64_decode(buf, buf + i + 1, num);

            if (ptr == NULL)
            {
                m_error = true;
                return false;
            }

            assert(ptr - buf == i + 1);
            assert(e::varint_length(*num) == i + 1);
            return true;
        }
    }

    m_error = true;
    return false;
}

extern "C"
{

YGOR_API ygor_data_logger*
ygor_data_logger_create(const char* output, uint64_t scale_when, uint64_t scale_data)
{
    ygor_data_logger* dl = new (std::nothrow) ygor_data_logger();

    if (!dl)
    {
        errno = ENOMEM;
        return NULL;
    }

    if (dl->open(output, scale_when, scale_data) < 0)
    {
        delete dl;
        return NULL;
    }

    return dl;
}

YGOR_API int
ygor_data_logger_flush_and_destroy(struct ygor_data_logger* dl)
{
    int x = dl->flush();
    int y = dl->close();
    delete dl;

    if (x < 0 || y < 0)
    {
        return -1;
    }

    return 0;
}

YGOR_API int
ygor_data_logger_start(struct ygor_data_logger*, struct ygor_data_record* dr)
{
    dr->when = po6::wallclock_time();
    dr->data = dr->when;
    return 0;
}

YGOR_API int
ygor_data_logger_finish(struct ygor_data_logger*, struct ygor_data_record* dr)
{
    dr->data = po6::wallclock_time() - dr->data;
    return 0;
}

YGOR_API int
ygor_data_logger_record(struct ygor_data_logger* dl, struct ygor_data_record* dr)
{
    return dl->record(dr);
}

YGOR_API ygor_data_iterator*
ygor_data_iterator_create(const char* input)
{
    ygor_data_iterator* di = new (std::nothrow) ygor_data_iterator();

    if (!di)
    {
        errno = ENOMEM;
        return NULL;
    }

    if (di->open(input) < 0)
    {
        delete di;
        return NULL;
    }

    return di;
}

YGOR_API void
ygor_data_iterator_destroy(struct ygor_data_iterator* di)
{
    if (di)
    {
        delete di;
    }
}

YGOR_API uint64_t
ygor_data_iterator_when_scale(struct ygor_data_iterator* ydi)
{
    return ydi->when_scale();
}

YGOR_API uint64_t
ygor_data_iterator_data_scale(struct ygor_data_iterator* ydi)
{
    return ydi->data_scale();
}

YGOR_API int
ygor_data_iterator_valid(struct ygor_data_iterator* di)
{
    if (di)
    {
        return di->valid();
    }
    else
    {
        return -1;
    }
}

YGOR_API void
ygor_data_iterator_advance(struct ygor_data_iterator* di)
{
    di->advance();
}

YGOR_API void
ygor_data_iterator_read(struct ygor_data_iterator* di,
                        struct ygor_data_record* dr)
{
    return di->read(dr);
}

YGOR_API int
ygor_unit_scaling_factor(const char* unit, uint64_t* nanos)
{
    if (*unit == '\0')
    {
        *nanos = 1;
        return 0;
    }
    if (strcmp(unit, "ns") == 0)
    {
        *nanos = 1;
        return 0;
    }
    if (strcmp(unit, "us") == 0)
    {
        *nanos = 1000ULL;
        return 0;
    }
    if (strcmp(unit, "ms") == 0)
    {
        *nanos = 1000ULL * 1000ULL;
        return 0;
    }
    if (strcmp(unit, "s") == 0)
    {
        *nanos = 1000ULL * 1000ULL * 1000ULL;
        return 0;
    }

    return -1;
}

YGOR_API int
ygor_bucket_size(const char* bucket, uint64_t* nanos)
{
    if (*bucket == '\0')
    {
        return -1;
    }

    char* end = NULL;
    *nanos = strtoull(bucket, &end, 10);

    if (end == bucket)
    {
        *nanos = 1;
    }

    uint64_t scale = 0;

    if (ygor_unit_scaling_factor(end, &scale) < 0)
    {
        return -1;
    }

    *nanos *= scale;
    return 0;
}

YGOR_API int
ygor_validate_bucket_units(uint64_t bucket_nanos, uint64_t unit_nanos)
{
    return bucket_nanos > 0 && roundup(bucket_nanos, unit_nanos) == bucket_nanos;
}

YGOR_API void
ygor_autoscale(double value, const char** str)
{
    value = fabs(value);

    if (value < 1000ULL)
    {
        *str = "ns";
    }
    else if (value < 1000ULL * 1000ULL)
    {
        *str = "us";
    }
    else if (value < 1000ULL * 1000ULL * 1000ULL)
    {
        *str = "ms";
    }
    else
    {
        *str = "s";
    }
}

YGOR_API int
ygor_data_logger_record_rusage_stats(struct ygor_data_logger* ydl, uint64_t when, struct rusage* usage)
{
    ygor_data_record ydr;
    ydr.when    = when;
    ydr.series  = SERIES_RU_UTIME;
    ydr.data    = usage->ru_utime.tv_sec * 1000000ULL + usage->ru_utime.tv_usec;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_STIME;
    ydr.data    = usage->ru_stime.tv_sec * 1000000ULL + usage->ru_stime.tv_usec;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_MAXRSS;
    ydr.data    = usage->ru_maxrss;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_IXRSS;
    ydr.data    = usage->ru_ixrss;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_IDRSS;
    ydr.data    = usage->ru_idrss;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_ISRSS;
    ydr.data    = usage->ru_isrss;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_MINFLT;
    ydr.data    = usage->ru_minflt;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_MAJFLT;
    ydr.data    = usage->ru_majflt;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_NSWAP;
    ydr.data    = usage->ru_nswap;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_INBLOCK;
    ydr.data    = usage->ru_inblock;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_OUBLOCK;
    ydr.data    = usage->ru_oublock;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_MSGSND;
    ydr.data    = usage->ru_msgsnd;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_MSGRCV;
    ydr.data    = usage->ru_msgrcv;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_NSIGNALS;
    ydr.data    = usage->ru_nsignals;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_NVCSW;
    ydr.data    = usage->ru_nvcsw;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_RU_NIVCSW;
    ydr.data    = usage->ru_nivcsw;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    return 0;
}

YGOR_API int
ygor_io_block_stat_path(const char* _path, char* stat_path, size_t stat_path_sz)
{
    std::string path(_path);

    if (!po6::path::realpath(path, &path))
    {
        return -1;
    }

    std::vector<std::string> block_devs;
    DIR* dir = opendir("/sys/block");
    struct dirent* ent = NULL;

    if (dir == NULL)
    {
        return -1;
    }

    errno = 0;

    while ((ent = readdir(dir)) != NULL)
    {
        block_devs.push_back(ent->d_name);
    }

    closedir(dir);

    if (errno != 0)
    {
        return -1;
    }

    FILE* mounts = fopen("/proc/mounts", "r");

    if (!mounts)
    {
        return -1;
    }

    char* line = NULL;
    size_t line_sz = 0;
    size_t max_mnt_sz = 0;
    bool done = false;
    bool error = false;

    while (!done && !error)
    {
        ssize_t amt = getline(&line, &line_sz, mounts);

        if (amt < 0)
        {
            if (ferror(mounts) != 0)
            {
                error = true;
                break;
            }

            if (feof(mounts) != 0)
            {
                break;
            }

            error = true;
            break;
        }

        char dev[4096];
        char mnt[4096];
        int pts = sscanf(line, "%4095s %4095s", dev, mnt);

        if (pts != 2)
        {
            continue;
        }

        size_t msz = strlen(mnt);

        if (strncmp(mnt, path.c_str(), msz) != 0 ||
            msz < max_mnt_sz)
        {
            continue;
        }

        std::string dev_path;

        if (po6::path::realpath(dev, &dev_path))
        {
            dev_path = po6::path::basename(dev_path);
        }
        else
        {
            dev_path = po6::path::basename(dev);
        }

        for (size_t i = 0; i < block_devs.size(); ++i)
        {
            size_t dsz = std::min(dev_path.size(), block_devs[i].size());

            if (strncmp(block_devs[i].c_str(), dev_path.c_str(), dsz) == 0)
            {
                max_mnt_sz = msz;
                std::string block_dev_path = std::string("/sys/block/") + block_devs[i] + "/stat";

                if (block_dev_path.size() + 1 < stat_path_sz)
                {
                    memmove(stat_path, block_dev_path.c_str(), block_dev_path.size() + 1);
                    done = true;
                }
            }
        }
    }

    if (line)
    {
        free(line);
    }

    fclose(mounts);
    return done ? 0 : -1;
}

YGOR_API int
ygor_io_collect_stats(char* path, struct ygor_io_stats* stats)
{
    FILE* fin = fopen(path, "r");

    if (!fin)
    {
        return -1;
    }

    uint64_t read_sectors;
    uint64_t write_sectors;
    int x = fscanf(fin, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &stats->read_ios, &stats->read_merges, &read_sectors, &stats->read_ticks,
                   &stats->write_ios, &stats->write_merges, &write_sectors, &stats->write_ticks,
                   &stats->in_flight, &stats->io_ticks, &stats->time_in_queue);
    fclose(fin);

    if (x == 11)
    {
        stats->read_bytes = read_sectors * 512;
        stats->write_bytes = write_sectors * 512;
        return 0;
    }

    return -1;
}

YGOR_API int
ygor_data_logger_record_io_stats(struct ygor_data_logger* ydl,
                                 uint64_t when,
                                 const struct ygor_io_stats* stats)
{
    ygor_data_record ydr;
    ydr.when    = when;
    ydr.series  = SERIES_IO_READ_IOS;
    ydr.data    = stats->read_ios;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_READ_MERGES;
    ydr.data    = stats->read_merges;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_READ_BYTES;
    ydr.data    = stats->read_bytes;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_READ_TICKS;
    ydr.data    = stats->read_ticks;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_WRITE_IOS;
    ydr.data    = stats->write_ios;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_WRITE_MERGES;
    ydr.data    = stats->write_merges;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_WRITE_BYTES;
    ydr.data    = stats->write_bytes;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_WRITE_TICKS;
    ydr.data    = stats->write_ticks;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_IN_FLIGHT;
    ydr.data    = stats->in_flight;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_IO_TICKS;
    ydr.data    = stats->io_ticks;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    ydr.series  = SERIES_IO_TIME_IN_QUEUE;
    ydr.data    = stats->time_in_queue;
    if (ygor_data_logger_record(ydl, &ydr) < 0) { return -1; }
    return 0;
}

YGOR_API int
ygor_cdf(const char* input, uint64_t nanos,
         struct ygor_data_point** data, uint64_t* data_sz)
{
    ygor_data_iterator* di = ygor_data_iterator_create(input);

    if (!di)
    {
        return -1;
    }

    std::vector<ygor_data_point> points;
    points.push_back(ygor_data_point());
    points[0].x = 0;
    points[0].y = 0;
    uint64_t num_points = 0;
    int status = 0;

    while ((status = ygor_data_iterator_valid(di)) > 0)
    {
        ygor_data_record dr;
        ygor_data_iterator_read(di, &dr);
        ygor_data_iterator_advance(di);
        size_t idx = 0;

        while (points[idx].x < dr.data)
        {
            if (idx + 1 == points.size())
            {
                ygor_data_point dp;
                dp.x = points[idx].x + nanos;
                dp.y = 0;
                points.push_back(dp);
            }

            ++idx;
        }

        points[idx].y += 1;
        ++num_points;
    }

    if (status < 0)
    {
        return false;
    }

    uint64_t sum = 0;
    size_t sz = sizeof(struct ygor_data_point) * points.size();
    *data = reinterpret_cast<struct ygor_data_point*>(malloc(sz));
    *data_sz = points.size();

    for (size_t i = 0; i < points.size(); ++i)
    {
        (*data)[i].x = points[i].x;
        sum += points[i].y;
        (*data)[i].y = 100. * static_cast<double>(sum)
                            / static_cast<double>(num_points);
    }

    return true;
}

YGOR_API int
ygor_timeseries(const char* input, uint64_t nanos,
                struct ygor_data_point** data, uint64_t* data_sz)
{
    ygor_data_iterator* di = ygor_data_iterator_create(input);

    if (!di)
    {
        return -1;
    }

    std::vector<ygor_data_point> points;
    int status = 0;

    while ((status = ygor_data_iterator_valid(di)) > 0)
    {
        ygor_data_record dr;
        ygor_data_iterator_read(di, &dr);
        ygor_data_iterator_advance(di);
        std::vector<ygor_data_point>::reverse_iterator rit = points.rbegin();

        while (rit != points.rend())
        {
            if (rit->x <= dr.when && dr.when < rit->x + nanos)
            {
                break;
            }

            ++rit;
        }

        if (rit == points.rend())
        {
            ygor_data_point dp;
            dp.x = dr.when - (dr.when % nanos);
            dp.y = 1;
            points.push_back(dp);
        }
        else
        {
            rit->y += 1;
        }
    }

    if (status < 0)
    {
        return false;
    }

    if (points.empty())
    {
        *data = NULL;
        *data_sz = 0;
        return true;
    }

    std::vector<ygor_data_point> dense_points;
    std::sort(points.begin(), points.end());
    dense_points.push_back(points[0]);

    for (size_t i = 1; i < points.size(); ++i)
    {
        while (dense_points.back().x < points[i].x)
        {
            ygor_data_point dp;
            dp.x = dense_points.back().x + nanos;
            dp.y = 0;
            dense_points.push_back(dp);
        }

        assert(dense_points.back().x == points[i].x);
        dense_points.back().y = points[i].y;
    }

    size_t sz = sizeof(struct ygor_data_point) * dense_points.size();
    *data = reinterpret_cast<struct ygor_data_point*>(malloc(sz));
    *data_sz = dense_points.size();

    for (size_t i = 0; i < dense_points.size(); ++i)
    {
        (*data)[i].x = dense_points[i].x - dense_points[0].x;
        (*data)[i].y = dense_points[i].y;
    }

    return true;
}

YGOR_API int
ygor_summarize(const char* input, struct ygor_summary* summary)
{
    ygor_data_iterator* di = ygor_data_iterator_create(input);

    if (!di)
    {
        return -1;
    }

    uint64_t start = UINT64_MAX;
    uint64_t end = 0;
    double n = 0;
    double mean = 0;
    double M2 = 0;
    int status = 0;

    while ((status = ygor_data_iterator_valid(di)) > 0)
    {
        ygor_data_record dr;
        ygor_data_iterator_read(di, &dr);
        ygor_data_iterator_advance(di);

        start = std::min(start, dr.when);
        end = std::max(end, dr.when);
        end = std::max(end, dr.when + dr.data);
        ++n;
        double delta = dr.data - mean;
        mean = mean + delta / n;
        M2 = M2 + delta * (dr.data - mean);
    }

    ygor_data_iterator_destroy(di);

    if (status < 0)
    {
        return false;
    }

    double stdev = 0;
    double variance = 0;

    if (n > 1)
    {
        variance = M2 / (n - 1);
        stdev = sqrt(variance);
    }

    summary->points = n;
    summary->nanos = end - start;
    summary->mean = mean;
    summary->stdev = stdev;
    summary->variance = variance;
    return true;
}

/* Tables and intent of throughput_latency_t_test borrowed from phk.
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */
#define NSTUDENT 100
#define NCONF 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlarger-than="
static double const studentpct[] = { 80, 90, 95, 98, 99, 99.5 };
static double student [NSTUDENT + 1][NCONF] = {
/* inf */   {   1.282,  1.645,  1.960,  2.326,  2.576,  3.090  },
/* 1. */    {   3.078,  6.314,  12.706, 31.821, 63.657, 318.313  },
/* 2. */    {   1.886,  2.920,  4.303,  6.965,  9.925,  22.327  },
/* 3. */    {   1.638,  2.353,  3.182,  4.541,  5.841,  10.215  },
/* 4. */    {   1.533,  2.132,  2.776,  3.747,  4.604,  7.173  },
/* 5. */    {   1.476,  2.015,  2.571,  3.365,  4.032,  5.893  },
/* 6. */    {   1.440,  1.943,  2.447,  3.143,  3.707,  5.208  },
/* 7. */    {   1.415,  1.895,  2.365,  2.998,  3.499,  4.782  },
/* 8. */    {   1.397,  1.860,  2.306,  2.896,  3.355,  4.499  },
/* 9. */    {   1.383,  1.833,  2.262,  2.821,  3.250,  4.296  },
/* 10. */   {   1.372,  1.812,  2.228,  2.764,  3.169,  4.143  },
/* 11. */   {   1.363,  1.796,  2.201,  2.718,  3.106,  4.024  },
/* 12. */   {   1.356,  1.782,  2.179,  2.681,  3.055,  3.929  },
/* 13. */   {   1.350,  1.771,  2.160,  2.650,  3.012,  3.852  },
/* 14. */   {   1.345,  1.761,  2.145,  2.624,  2.977,  3.787  },
/* 15. */   {   1.341,  1.753,  2.131,  2.602,  2.947,  3.733  },
/* 16. */   {   1.337,  1.746,  2.120,  2.583,  2.921,  3.686  },
/* 17. */   {   1.333,  1.740,  2.110,  2.567,  2.898,  3.646  },
/* 18. */   {   1.330,  1.734,  2.101,  2.552,  2.878,  3.610  },
/* 19. */   {   1.328,  1.729,  2.093,  2.539,  2.861,  3.579  },
/* 20. */   {   1.325,  1.725,  2.086,  2.528,  2.845,  3.552  },
/* 21. */   {   1.323,  1.721,  2.080,  2.518,  2.831,  3.527  },
/* 22. */   {   1.321,  1.717,  2.074,  2.508,  2.819,  3.505  },
/* 23. */   {   1.319,  1.714,  2.069,  2.500,  2.807,  3.485  },
/* 24. */   {   1.318,  1.711,  2.064,  2.492,  2.797,  3.467  },
/* 25. */   {   1.316,  1.708,  2.060,  2.485,  2.787,  3.450  },
/* 26. */   {   1.315,  1.706,  2.056,  2.479,  2.779,  3.435  },
/* 27. */   {   1.314,  1.703,  2.052,  2.473,  2.771,  3.421  },
/* 28. */   {   1.313,  1.701,  2.048,  2.467,  2.763,  3.408  },
/* 29. */   {   1.311,  1.699,  2.045,  2.462,  2.756,  3.396  },
/* 30. */   {   1.310,  1.697,  2.042,  2.457,  2.750,  3.385  },
/* 31. */   {   1.309,  1.696,  2.040,  2.453,  2.744,  3.375  },
/* 32. */   {   1.309,  1.694,  2.037,  2.449,  2.738,  3.365  },
/* 33. */   {   1.308,  1.692,  2.035,  2.445,  2.733,  3.356  },
/* 34. */   {   1.307,  1.691,  2.032,  2.441,  2.728,  3.348  },
/* 35. */   {   1.306,  1.690,  2.030,  2.438,  2.724,  3.340  },
/* 36. */   {   1.306,  1.688,  2.028,  2.434,  2.719,  3.333  },
/* 37. */   {   1.305,  1.687,  2.026,  2.431,  2.715,  3.326  },
/* 38. */   {   1.304,  1.686,  2.024,  2.429,  2.712,  3.319  },
/* 39. */   {   1.304,  1.685,  2.023,  2.426,  2.708,  3.313  },
/* 40. */   {   1.303,  1.684,  2.021,  2.423,  2.704,  3.307  },
/* 41. */   {   1.303,  1.683,  2.020,  2.421,  2.701,  3.301  },
/* 42. */   {   1.302,  1.682,  2.018,  2.418,  2.698,  3.296  },
/* 43. */   {   1.302,  1.681,  2.017,  2.416,  2.695,  3.291  },
/* 44. */   {   1.301,  1.680,  2.015,  2.414,  2.692,  3.286  },
/* 45. */   {   1.301,  1.679,  2.014,  2.412,  2.690,  3.281  },
/* 46. */   {   1.300,  1.679,  2.013,  2.410,  2.687,  3.277  },
/* 47. */   {   1.300,  1.678,  2.012,  2.408,  2.685,  3.273  },
/* 48. */   {   1.299,  1.677,  2.011,  2.407,  2.682,  3.269  },
/* 49. */   {   1.299,  1.677,  2.010,  2.405,  2.680,  3.265  },
/* 50. */   {   1.299,  1.676,  2.009,  2.403,  2.678,  3.261  },
/* 51. */   {   1.298,  1.675,  2.008,  2.402,  2.676,  3.258  },
/* 52. */   {   1.298,  1.675,  2.007,  2.400,  2.674,  3.255  },
/* 53. */   {   1.298,  1.674,  2.006,  2.399,  2.672,  3.251  },
/* 54. */   {   1.297,  1.674,  2.005,  2.397,  2.670,  3.248  },
/* 55. */   {   1.297,  1.673,  2.004,  2.396,  2.668,  3.245  },
/* 56. */   {   1.297,  1.673,  2.003,  2.395,  2.667,  3.242  },
/* 57. */   {   1.297,  1.672,  2.002,  2.394,  2.665,  3.239  },
/* 58. */   {   1.296,  1.672,  2.002,  2.392,  2.663,  3.237  },
/* 59. */   {   1.296,  1.671,  2.001,  2.391,  2.662,  3.234  },
/* 60. */   {   1.296,  1.671,  2.000,  2.390,  2.660,  3.232  },
/* 61. */   {   1.296,  1.670,  2.000,  2.389,  2.659,  3.229  },
/* 62. */   {   1.295,  1.670,  1.999,  2.388,  2.657,  3.227  },
/* 63. */   {   1.295,  1.669,  1.998,  2.387,  2.656,  3.225  },
/* 64. */   {   1.295,  1.669,  1.998,  2.386,  2.655,  3.223  },
/* 65. */   {   1.295,  1.669,  1.997,  2.385,  2.654,  3.220  },
/* 66. */   {   1.295,  1.668,  1.997,  2.384,  2.652,  3.218  },
/* 67. */   {   1.294,  1.668,  1.996,  2.383,  2.651,  3.216  },
/* 68. */   {   1.294,  1.668,  1.995,  2.382,  2.650,  3.214  },
/* 69. */   {   1.294,  1.667,  1.995,  2.382,  2.649,  3.213  },
/* 70. */   {   1.294,  1.667,  1.994,  2.381,  2.648,  3.211  },
/* 71. */   {   1.294,  1.667,  1.994,  2.380,  2.647,  3.209  },
/* 72. */   {   1.293,  1.666,  1.993,  2.379,  2.646,  3.207  },
/* 73. */   {   1.293,  1.666,  1.993,  2.379,  2.645,  3.206  },
/* 74. */   {   1.293,  1.666,  1.993,  2.378,  2.644,  3.204  },
/* 75. */   {   1.293,  1.665,  1.992,  2.377,  2.643,  3.202  },
/* 76. */   {   1.293,  1.665,  1.992,  2.376,  2.642,  3.201  },
/* 77. */   {   1.293,  1.665,  1.991,  2.376,  2.641,  3.199  },
/* 78. */   {   1.292,  1.665,  1.991,  2.375,  2.640,  3.198  },
/* 79. */   {   1.292,  1.664,  1.990,  2.374,  2.640,  3.197  },
/* 80. */   {   1.292,  1.664,  1.990,  2.374,  2.639,  3.195  },
/* 81. */   {   1.292,  1.664,  1.990,  2.373,  2.638,  3.194  },
/* 82. */   {   1.292,  1.664,  1.989,  2.373,  2.637,  3.193  },
/* 83. */   {   1.292,  1.663,  1.989,  2.372,  2.636,  3.191  },
/* 84. */   {   1.292,  1.663,  1.989,  2.372,  2.636,  3.190  },
/* 85. */   {   1.292,  1.663,  1.988,  2.371,  2.635,  3.189  },
/* 86. */   {   1.291,  1.663,  1.988,  2.370,  2.634,  3.188  },
/* 87. */   {   1.291,  1.663,  1.988,  2.370,  2.634,  3.187  },
/* 88. */   {   1.291,  1.662,  1.987,  2.369,  2.633,  3.185  },
/* 89. */   {   1.291,  1.662,  1.987,  2.369,  2.632,  3.184  },
/* 90. */   {   1.291,  1.662,  1.987,  2.368,  2.632,  3.183  },
/* 91. */   {   1.291,  1.662,  1.986,  2.368,  2.631,  3.182  },
/* 92. */   {   1.291,  1.662,  1.986,  2.368,  2.630,  3.181  },
/* 93. */   {   1.291,  1.661,  1.986,  2.367,  2.630,  3.180  },
/* 94. */   {   1.291,  1.661,  1.986,  2.367,  2.629,  3.179  },
/* 95. */   {   1.291,  1.661,  1.985,  2.366,  2.629,  3.178  },
/* 96. */   {   1.290,  1.661,  1.985,  2.366,  2.628,  3.177  },
/* 97. */   {   1.290,  1.661,  1.985,  2.365,  2.627,  3.176  },
/* 98. */   {   1.290,  1.661,  1.984,  2.365,  2.627,  3.175  },
/* 99. */   {   1.290,  1.660,  1.984,  2.365,  2.626,  3.175  },
/* 100. */  {   1.290,  1.660,  1.984,  2.364,  2.626,  3.174  }
};
#pragma GCC diagnostic pop

YGOR_API int
ygor_t_test(struct ygor_summary* rs,
            struct ygor_summary* ds,
            double interval,
            struct ygor_difference* diff)
{
    int confidx = -1;

    for (size_t i = 0; i < NCONF; ++i)
    {
        if (studentpct[i] * 0.99 < interval &&
            studentpct[i] * 1.01 > interval)
        {
            confidx = i;
            break;
        }
    }

    if (confidx < 0)
    {
        return -1;
    }

    double spool, s, d, e, t;
    uint64_t i;

    i = ds->points + rs->points - 2;

    if (i > NSTUDENT)
    {
        t = student[0][confidx];
    }
    else
    {
        t = student[1][confidx];
    }

    spool = (ds->points - 1) * ds->variance + (rs->points - 1) * rs->variance;
    spool /= ds->points + rs->points - 2;
    spool = sqrt(spool);
    s = spool * sqrt(1.0 / ds->points + 1.0 / rs->points);
    d = ds->mean - rs->mean;
    e = t * s;
    diff->raw = d;
    diff->raw_plus_minus = e;
    diff->percent = d * 100 / (rs->mean);
    diff->percent_plus_minus = e * 100 / rs->mean;
    return fabs(d) > e ? 1 : 0;
}

} // extern "C"
