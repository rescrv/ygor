// Copyright (c) 2017, Robert Escriva
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
#include <stdint.h>

// po6
#include <po6/threads/thread.h>
#include <po6/time.h>

// e
#include <e/atomic.h>
#include <e/compat.h>
#include <e/popt.h>

// ygor
#include <ygor/data.h>

uint32_t done = 0;

void
recorder(ygor_data_logger* ydl, ygor_series* s, long usleep)
{
    ygor_data_point ydp;
    ydp.series = s;
    ydp.indep.precise = po6::wallclock_time() / PO6_MILLIS;
    ydp.dep.approximate = 1;

    while (!e::atomic::load_32_nobarrier(&done))
    {
        uint64_t start = po6::wallclock_time();

        if (usleep > 0)
        {
            po6::sleep(usleep * PO6_MICROS);
        }

        int ret = ygor_data_logger_record(ydl, &ydp);
        uint64_t end = po6::wallclock_time();
        assert(ret == 0);
        ydp.indep.precise = end / PO6_MILLIS;
        ydp.dep.approximate = (end - start) / (double)PO6_MICROS;
    }
}

bool
choose_precision(const char* s, ygor_precision* p)
{
    if (strcmp("precise", s) == 0)
    {
        *p = YGOR_PRECISE_INTEGER;
        return true;
    }
    if (strcmp("half", s) == 0 ||
        strcmp("half-precision", s) == 0)
    {
        *p = YGOR_HALF_PRECISION;
        return true;
    }
    if (strcmp("single", s) == 0 ||
        strcmp("single-precision", s) == 0)
    {
        *p = YGOR_SINGLE_PRECISION;
        return true;
    }
    if (strcmp("double", s) == 0 ||
        strcmp("double-precision", s) == 0)
    {
        *p = YGOR_DOUBLE_PRECISION;
        return true;
    }

    return false;
}

int
main(int argc, const char* argv[])
{
    long numthreads = 1;
    long usleeps = 0;
    const char* output = "benchmark.dat";
    const char* precision = "double";
    e::argparser ap;
    ap.autohelp();
    ap.arg().name('t', "threads")
            .description("number of threads to run (default: 1)")
            .as_long(&numthreads);
    ap.arg().name('s', "sleep")
            .description("number of microseconds to sleep per op (default: don't)")
            .as_long(&usleeps);
    ap.arg().name('o', "output")
            .description("output file (default: benchmark.dat")
            .as_string(&output);
    ap.arg().name('p', "precision")
            .description("precision of the measurements (default: precise)")
            .as_string(&precision);

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (ap.args_sz() != 0)
    {
        fprintf(stderr, "command takes no positional arguments\n");
        ap.usage();
        return EXIT_FAILURE;
    }

    ygor_series s;
    s.name = "overhead";
    s.indep_units = YGOR_UNIT_MS;
    s.indep_precision = YGOR_PRECISE_INTEGER;
    s.dep_units = YGOR_UNIT_US;
    s.dep_precision = YGOR_DOUBLE_PRECISION;

    if (!choose_precision(precision, &s.dep_precision))
    {
        return EXIT_FAILURE;
    }

    const ygor_series* ss[] = {&s};
    ygor_data_logger* ydl = ygor_data_logger_create(output, ss, 1);

    if (!ydl)
    {
        fprintf(stderr, "could not create data logger\n");
        return EXIT_FAILURE;
    }

    std::vector<e::compat::shared_ptr<po6::threads::thread> > threads;

    for (long i = 0; i < numthreads; ++i)
    {
        using namespace po6::threads;
        e::compat::shared_ptr<thread> t(new thread(make_func(recorder, ydl, &s, usleeps)));
        threads.push_back(t);
    }

    for (long i = 0; i < numthreads; ++i)
    {
        threads[i]->start();
    }

    po6::sleep(60 * PO6_SECONDS);
    e::atomic::store_32_release(&done, 1);

    for (long i = 0; i < numthreads; ++i)
    {
        threads[i]->join();
    }

    if (ygor_data_logger_flush_and_destroy(ydl) < 0)
    {
        fprintf(stderr, "could not flush data logger\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
