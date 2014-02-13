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
//     * Neither the name of this project nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
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
    ap.option_string("<input file> [<input file> ...]");
    bucket_scale_opts bso;
    ap.add("Units:", bso.parser());

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (!bso.validate())
    {
        return EXIT_FAILURE;
    }

    if (ap.args_sz() < 1)
    {
        fprintf(stderr, "specify at least one input file\n");
        return EXIT_FAILURE;
    }

    std::vector<ygor_series> timeseries;
    timeseries.resize(ap.args_sz());
    size_t max_idx = 0;

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        if (ygor_timeseries(ap.args()[i], bso.bucket(), &timeseries[i].data, &timeseries[i].data_sz) < 0)
        {
            fprintf(stderr, "cannot create timeseries from input %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }

        max_idx = std::max(max_idx, timeseries[i].data_sz);
    }

    for (size_t idx = 0; idx < max_idx; ++idx)
    {
        fprintf(stdout, "%ld", bso.index_scaled(idx));

        for (size_t i = 0; i < timeseries.size(); ++i)
        {
            if (idx < timeseries[i].data_sz)
            {
                assert(timeseries[i].data[idx].x == bso.index(idx));
                fprintf(stdout, "\t%g", timeseries[i].data[idx].y);
            }
            else
            {
                fprintf(stdout, "\t-");
            }
        }

        fprintf(stdout, "\n");
    }

    return EXIT_SUCCESS;
}
