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
#include <cmath>
#include <cstdlib>
#include <stdint.h>

// e
#include <e/popt.h>

// ygor
#include <ygor/data.h>
#include "common.h"

int
main(int argc, const char* argv[])
{
    e::argparser ap;
    ap.autohelp();
    ap.option_string("[<data-set> ...]");

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    int rc = EXIT_SUCCESS;

    for (size_t i = 0; i < ap.args_sz(); ++i)
    {
        ygor_data_reader* ydr = ygor_data_reader_create(ap.args()[i]);

        if (!ydr)
        {
            fprintf(stderr, "could not open or parse %s\n", ap.args()[i]);
            rc = EXIT_FAILURE;
            continue;
        }

        size_t series_sz = ygor_data_reader_num_series(ydr);

        for (size_t idx = 0; idx < series_sz; ++idx)
        {
            const ygor_series* s = ygor_data_reader_series(ydr, idx);
            printf("%s: independent=%s@%s dependent=%s@%s\n",
                   s->name,
                   units_to_str(s->indep_units),
                   precision_to_str(s->indep_precision),
                   units_to_str(s->dep_units),
                   precision_to_str(s->dep_precision));
        }
    }

    return rc;
}
