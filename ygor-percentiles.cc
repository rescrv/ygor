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

    ygor_data_point* data = NULL;
    size_t data_sz = 0;

    if (ygor_cdf(ap.args()[0], bso.bucket(), &data, &data_sz) < 0)
    {
        fprintf(stderr, "cannot read from input %s\n", ap.args()[0]);
        return EXIT_FAILURE;
    }

    for (size_t i = 1; i < ap.args_sz(); ++i)
    {
        if (i > 1)
        {
            fprintf(stdout, " ");
        }

        char* end = NULL;
        uint64_t percentile = strtol(ap.args()[i], &end, 10);

        if (!end || *end != '\0' || percentile > 100)
        {
            fprintf(stderr, "percentile must be in range [0:100]\n");
            return EXIT_FAILURE;

        }

        for (size_t idx = 0; idx < data_sz; ++idx)
        {
            if (data[idx].y >= percentile)
            {
                fprintf(stdout, "%ld", bso.index_scaled(idx));
                break;
            }
        }
    }

    if (ap.args_sz() > 1)
    {
        fprintf(stdout, "\n");
    }

    return EXIT_SUCCESS;
}
