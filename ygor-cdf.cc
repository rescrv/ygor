// Copyright (c) 2013-2017, Robert Escriva
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

// e
#include <e/popt.h>

// ygor
#include <ygor/data.h>
#include "common.h"

int
main(int argc, const char* argv[])
{
    long fill = 0;
    bool omit_empty = false;
    e::argparser ap;
    ap.autohelp();
    ap.option_string("<input> [<input> ...]");
    ap.arg().name('e', "omit-empty").description("Omit empty data points (default: treat as 100%)").set_true(&omit_empty);
    ap.arg().name('f', "fill").description("Appends 100%% up to the given bucket (default: no fill)").as_long(&fill);
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

    if (ap.args_sz() < 1)
    {
        fprintf(stderr, "specify at least one input file\n");
        return EXIT_FAILURE;
    }

    std::vector<series_description> series = compute_series(ap.args(), ap.args_sz());
    std::vector<data_points> cdfs;
    uint64_t max_idx = 0;

    for (size_t i = 0; i < series.size(); ++i)
    {
        ygor_data_reader* ydr = NULL;
        ygor_data_iterator* ydi = NULL;
        ygor_data_point* ydp;
        size_t ydp_sz;

        if (!(ydr = ygor_data_reader_create(series[i].filename.c_str())) ||
            !(ydi = ygor_data_iterate(ydr, series[i].series_name.c_str())) ||
            !(ydi = ygor_data_convert_units(ydi, ygor_data_iterator_series(ydi)->indep_units, bopts.units())) ||
            ygor_cdf(ydi, bopts.bucket(), &ydp, &ydp_sz) < 0)
        {
            fprintf(stderr, "cannot create CDF from input %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }

        cdfs.push_back(data_points(ydp, ydp_sz));
        max_idx = std::max(max_idx, cdfs[i].data_sz);
        ygor_data_iterator_destroy(ydi);
        ygor_data_reader_destroy(ydr);
    }

    for (uint64_t idx = 0; ; ++idx)
    {
        if (idx >= max_idx && bopts.bucket() * idx > (unsigned long)fill)
        {
            break;
        }

        fprintf(stdout, "%ld", bopts.bucket() * idx);

        for (size_t i = 0; i < cdfs.size(); ++i)
        {
            if (idx < cdfs[i].data_sz)
            {
                assert(cdfs[i].data[idx].indep.precise == bopts.bucket() * idx);
                fprintf(stdout, "\t%g", cdfs[i].data[idx].dep.approximate);
            }
            else
            {
                fprintf(stdout, omit_empty ? "\t-" : "\t100");
            }
        }

        fprintf(stdout, "\n");
    }

    return EXIT_SUCCESS;
}
