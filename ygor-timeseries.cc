// Copyright (c) 2013,2017, Robert Escriva
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

// ygor
#include <ygor/data.h>
#include "common.h"

int
main(int argc, const char* argv[])
{
    e::argparser ap;
    ap.autohelp();
    ap.option_string("<input> [<input> ...]");
    bucket_options bopts;
    ap.add("Bucket options:", bopts.parser());

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (!bopts.validate())
    {
        return EXIT_FAILURE;
    }

    if (ap.args_sz() != 1)
    {
        fprintf(stderr, "specify just one input file\n");
        return EXIT_FAILURE;
    }

    std::vector<series_description> series = compute_series(ap.args(), ap.args_sz());
    std::vector<data_points> timeseries;

    for (size_t i = 0; i < series.size(); ++i)
    {
        ygor_data_reader* ydr = NULL;
        ygor_data_iterator* ydi = NULL;
        ygor_data_point* ydp;
        size_t ydp_sz;

        if (!(ydr = ygor_data_reader_create(series[i].filename.c_str())) ||
            !(ydi = ygor_data_iterate(ydr, series[i].series_name.c_str())) ||
            !(ydi = ygor_data_convert_units(ydi, bopts.units(), ygor_data_iterator_series(ydi)->dep_units)) ||
            ygor_timeseries(ydi, bopts.bucket(), &ydp, &ydp_sz) < 0)
        {
            fprintf(stderr, "cannot create timeseries from input %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }

        timeseries.push_back(data_points(ydp, ydp_sz));
        ygor_data_iterator_destroy(ydi);
        ygor_data_reader_destroy(ydr);

        for (size_t j = 0; j < ydp_sz; ++j)
        {
            printf("%lu %lu\n", ydp[j].indep.precise, ydp[j].dep.precise);
        }
    }

    return EXIT_SUCCESS;
}
