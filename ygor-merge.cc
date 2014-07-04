// Copyright (c) 2014, Robert Escriva
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

// C
#include <cstdlib>

// POSIX
#include <sys/stat.h>

// STL
#include <algorithm>

// Ygor
#include "ygor.h"
#include "ygor-internal.h"

typedef std::pair<ygor_data_record, ygor_data_iterator*> heap_elem;

bool
heap_func(const heap_elem& lhs, const heap_elem& rhs)
{
    return compare_by_series_then_when(rhs.first, lhs.first);
}

int
main(int argc, const char* argv[])
{
    const char* out = "merged.dat.bz2";
    bool has_out = false;
    bool overwrite = false;
    e::argparser ap;
    ap.autohelp();
    ap.option_string("[<data-set> ...]");
    ap.arg().name('o', "output")
            .description("output file (default: merged.dat.bz2)")
            .as_string(&out).set_true(&has_out);
    ap.arg().name('f', "force")
            .description("overwrite the output file if it exists")
            .set_true(&overwrite);

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

    if (ap.args_sz() == 0)
    {
        return EXIT_SUCCESS;
    }

    std::vector<ygor_data_iterator*> iters;

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        iters.push_back(ygor_data_iterator_create(ap.args()[i]));

        if (!iters[i])
        {
            fprintf(stderr, "could not open input file %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }
    }

    const uint64_t when_scale = ygor_data_iterator_when_scale(iters[0]);
    const uint64_t data_scale = ygor_data_iterator_data_scale(iters[0]);

    for (size_t i = 1; i < ap.args_sz(); ++i)
    {
        if (when_scale != ygor_data_iterator_when_scale(iters[i]) ||
            data_scale != ygor_data_iterator_data_scale(iters[i]))
        {
            fprintf(stderr, "cannot merge files with different time/data scales\n");
            return EXIT_FAILURE;
        }
    }

    struct ygor_data_logger* ydl = ygor_data_logger_create(out, when_scale, data_scale);

    if (!ydl)
    {
        fprintf(stderr, "could not open output file %s\n", out);
        return EXIT_FAILURE;
    }

    std::vector<heap_elem> heap;

    for (size_t idx = 0; idx < iters.size(); ++idx)
    {
        int valid = ygor_data_iterator_valid(iters[idx]);

        if (valid > 0)
        {
            ygor_data_record ydr;
            ygor_data_iterator_read(iters[idx], &ydr);
            heap.push_back(std::make_pair(ydr, iters[idx]));
        }
        else if (valid < 0)
        {
            fprintf(stderr, "error reading input\n");
            return EXIT_FAILURE;
        }
    }

    std::make_heap(heap.begin(), heap.end(), heap_func);

    while (!heap.empty())
    {
        heap_elem e = heap[0];
        std::pop_heap(heap.begin(), heap.end(), heap_func);
        heap.pop_back();

        if (ygor_data_logger_record(ydl, &e.first) < 0)
        {
            fprintf(stderr, "error writing output\n");
            return EXIT_FAILURE;
        }

        ygor_data_iterator_advance(e.second);
        int valid = ygor_data_iterator_valid(e.second);

        if (valid > 0)
        {
            ygor_data_record ydr;
            ygor_data_iterator_read(e.second, &ydr);
            heap.push_back(std::make_pair(ydr, e.second));
            std::push_heap(heap.begin(), heap.end(), heap_func);
        }
        else if (valid < 0)
        {
            fprintf(stderr, "error reading input\n");
            return EXIT_FAILURE;
        }
    }

    if (ygor_data_logger_flush_and_destroy(ydl) < 0)
    {
        fprintf(stderr, "error writing output\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
