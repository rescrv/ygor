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

// C
#include <cmath>
#include <cstdlib>
#include <stdint.h>

// e
#include <e/popt.h>

// Ygor
#include "ygor.h"
#include "ygor-internal.h"

int
main(int argc, const char* argv[])
{
    e::argparser ap;
    ap.autohelp();
    ap.option_string("[<data-set> ...]");
    scale_opts so;
    ap.add("Units:", so.parser());

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    std::vector<ygor_summary> summs;
    summs.resize(ap.args_sz());

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        if (ygor_summarize(ap.args()[i], &summs[i]) < 0)
        {
            fprintf(stderr, "cannot summarize input %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }
    }

    size_t input_sz = 0;

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        uint64_t scale;
        const char* scale_str;

        if (!so.scale(summs[i].mean, &scale, &scale_str))
        {
            return EXIT_FAILURE;
        }

        input_sz = std::max(input_sz, strlen(ap.args()[i]));
    }

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        uint64_t scale;
        const char* scale_str;

        if (!so.scale(summs[i].mean, &scale, &scale_str))
        {
            return EXIT_FAILURE;
        }

        fprintf(stdout, "%-*s n=%ld time=%lu%s throughput=%g mean=%g%s stdev=%g%s\n",
                        int(input_sz), ap.args()[i], summs[i].points,
                        summs[i].nanos / scale, scale_str,
                        (summs[i].points * 1000ULL * 1000ULL * 1000.)/ summs[i].nanos,
                        summs[i].mean / scale, scale_str,
                        summs[i].stdev / scale, scale_str);
    }

    return EXIT_SUCCESS;
}
