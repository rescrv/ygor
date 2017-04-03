// Copyright (c) 2014,2017, Robert Escriva
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

// C
#include <cstdlib>

// POSIX
#include <sys/stat.h>

// STL
#include <algorithm>

// ygor
#include <ygor/data.h>
#include "common.h"
#include "ygor-internal.h"

bool
series_same_name(const ygor_series& lhs, const ygor_series& rhs)
{
    return strcmp(lhs.name, rhs.name) == 0;
}

bool
series_equal(const ygor_series& lhs, const ygor_series& rhs)
{
    return strcmp(lhs.name, rhs.name) == 0 &&
           lhs.indep_units == rhs.indep_units &&
           lhs.indep_precision == rhs.indep_precision &&
           lhs.dep_units == rhs.dep_units &&
           lhs.dep_precision == rhs.dep_precision;
}

bool
has_series(ygor_data_reader* ydr, const char* name)
{
    for (size_t i = 0; i < ygor_data_reader_num_series(ydr); ++i)
    {
        if (strcmp(ygor_data_reader_series(ydr, i)->name, name) == 0)
        {
            return true;
        }
    }

    return false;
}

bool
conflicting_series(const std::vector<const ygor_series*> series, const ygor_series* s)
{
    for (size_t i = 0; i < series.size(); ++i)
    {
        if (strcmp(s->name, series[i]->name) == 0 && !series_equal(*s, *series[i]))
        {
            return true;
        }
    }

    return false;
}

bool
series_defined(const std::vector<const ygor_series*> series, const char* name)
{
    for (size_t i = 0; i < series.size(); ++i)
    {
        if (strcmp(name, series[i]->name) == 0)
        {
            return true;
        }
    }

    return false;
}

bool
precise_heap_func(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.indep.precise > rhs.indep.precise;
}

bool
approximate_heap_func(const ygor_data_point& lhs, const ygor_data_point& rhs)
{
    return lhs.indep.approximate > rhs.indep.approximate;
}

int
main(int argc, const char* argv[])
{
    const char* out = "merged.dat";
    bool has_out = false;
    bool overwrite = false;
    long buffer_sz = 64;
    e::argparser ap;
    ap.autohelp();
    ap.option_string("<input> [<input> ...]");
    ap.arg().name('o', "output")
            .description("output file (default: merged.dat)")
            .as_string(&out).set_true(&has_out);
    ap.arg().name('f', "force")
            .description("overwrite the output file if it exists")
            .set_true(&overwrite);
    ap.arg().name('b', "buffer")
            .description("buffer size (in MB) to use for in-memory sorting (default: 64)")
            .as_long(&buffer_sz);

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    struct stat x;
    int rc = lstat(out, &x);

    if (rc == 0 && !overwrite)
    {
        fprintf(stderr, "output file already exists and --force was not specified\n");
        return EXIT_FAILURE;
    }

    std::vector<ygor_data_reader*> readers;
    std::vector<const ygor_series*> series;

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        ygor_data_reader* ydr = ygor_data_reader_create(ap.args()[i]);

        if (!ydr)
        {
            fprintf(stderr, "could not open input file %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }

        readers.push_back(ydr);

        for (size_t j = 0; j < ygor_data_reader_num_series(ydr); ++j)
        {
            const ygor_series* s = ygor_data_reader_series(ydr, j);

            if (conflicting_series(series, s))
            {
                fprintf(stderr, "series %s has conflicting definitions\n", s->name);
                return EXIT_FAILURE;
            }

            if (!series_defined(series, s->name))
            {
                series.push_back(s);
            }
        }
    }

    const uint64_t heap_max = buffer_sz * 1048576ULL / sizeof(ygor_data_point) + 1;
    std::cout << "heap max " << heap_max << std::endl;
    ygor_data_logger* ydl = ygor_data_logger_create(out, &series[0], series.size());

    if (!ydl)
    {
        fprintf(stderr, "could not open output file %s\n", out);
        return EXIT_FAILURE;
    }

    for (size_t s = 0; s < series.size(); ++s)
    {
        bool (*heap_func)(const ygor_data_point& lhs, const ygor_data_point& rhs);

        if (ygor_is_precise(series[s]->indep_precision))
        {
            heap_func = precise_heap_func;
        }
        else
        {
            heap_func = approximate_heap_func;
        }

        std::vector<ygor_data_point> points;

        for (size_t r = 0; r < readers.size(); ++r)
        {
            if (!has_series(readers[r], series[s]->name))
            {
                continue;
            }

            ygor_data_iterator* ydi = ygor_data_iterate(readers[r], series[s]->name);

            if (!ydi)
            {
                fprintf(stderr, "could not open series %s:%s\n", ap.args()[r], series[s]->name);
                return EXIT_FAILURE;
            }

            int status;

            while ((status = ygor_data_iterator_valid(ydi)) > 0)
            {
                ygor_data_point ydp;
                ygor_data_iterator_read(ydi, &ydp);
                ygor_data_iterator_advance(ydi);
                assert(series_equal(*ydp.series, *series[s]));
                ydp.series = series[s];
                points.push_back(ydp);
                std::push_heap(points.begin(), points.end(), heap_func);

                if (points.size() > heap_max)
                {
                    std::pop_heap(points.begin(), points.end(), heap_func);

                    if (ygor_data_logger_record(ydl, &points.back()) < 0)
                    {
                        fprintf(stderr, "error writing output\n");
                        return EXIT_FAILURE;
                    }

                    points.pop_back();
                }
            }

            if (status < 0)
            {
                fprintf(stderr, "error reading series %s:%s\n", ap.args()[r], series[s]->name);
                return EXIT_FAILURE;
            }
        }

        while (!points.empty())
        {
            std::pop_heap(points.begin(), points.end(), heap_func);

            if (ygor_data_logger_record(ydl, &points.back()) < 0)
            {
                fprintf(stderr, "error writing output\n");
                return EXIT_FAILURE;
            }

            points.pop_back();
        }
    }

    if (ygor_data_logger_flush_and_destroy(ydl) < 0)
    {
        fprintf(stderr, "error writing output\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
