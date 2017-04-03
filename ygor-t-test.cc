// Copyright (c) 2013, Robert Escriva
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
#include <cmath>
#include <cstdlib>
#include <stdint.h>

// e
#include <e/popt.h>

// ygor
#include "ygor.h"
#include "ygor-internal.h"

int
main(int argc, const char* argv[])
{
    double interval = 95;
    e::argparser ap;
    ap.autohelp();
    ap.option_string("<baseline> [<comparison> ...]");
    ap.arg().name('i', "interval").description("Confidence interval in percent as an integer (default: 95%)").as_double(&interval);
    scale_opts so;
    ap.add("Units:", so.parser());

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (ap.args_sz() < 1)
    {
        fprintf(stderr, "specify at least one input file\n");
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

    fprintf(stdout, "baseline is %s\n", ap.args()[0]);

    for (size_t i = 1; i < ap.args_sz(); ++i)
    {
        ygor_difference diff;
        int cmp = ygor_t_test(&summs[0], &summs[i], interval, &diff);

        if (cmp > 0)
        {
            uint64_t scale;
            const char* scale_str;

            if (!so.scale(diff.raw, &scale, &scale_str))
            {
                return EXIT_FAILURE;
            }

            fprintf(stdout, "%s: difference at %g confidence => %g%s +/- %g%s, %g%% +/- %g%%\n",
                            ap.args()[i], interval,
                            diff.raw / scale, scale_str, diff.raw_plus_minus / scale, scale_str,
                            diff.percent, diff.percent_plus_minus);
        }
        else if (cmp == 0)
        {
            fprintf(stdout, "%s: no difference at %g confidence\n",
                            ap.args()[i], interval);
        }
        else
        {
            fprintf(stdout, "invalid confidence interval\n");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
