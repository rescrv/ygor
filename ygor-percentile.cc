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

// e
#include <e/popt.h>

// ygor
#include <ygor/data.h>
#include "common.h"

bool
parse_percentiles(const char* pcs_str, std::vector<double>* percentiles)
{
    while (true)
    {
        errno = 0;
        char* end = NULL;
        double d = strtod(pcs_str, &end);

        if (errno || end == pcs_str)
        {
            fprintf(stderr, "could not interpret percentile string\n");
            return false;
        }

        percentiles->push_back(d);

        if (*end == '\0')
        {
            return true;
        }
        else if (*end == ',')
        {
            pcs_str = end + 1;
        }
        else
        {
            fprintf(stderr, "could not interpret percentile string\n");
            return false;
        }
    }
}

int
main(int argc, const char* argv[])
{
    const char* pcs_str = ".5,.95,.99";
    e::argparser ap;
    ap.autohelp();
    ap.option_string("<input> [<input> ...]");
    ap.arg().name('p', "percentiles")
            .description("Comma separated list of percentile values (default: .5,.95,.99)")
            .as_string(&pcs_str);
    scale_options sopts;
    ap.add("Scale options:", sopts.parser());

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (!sopts.validate())
    {
        return EXIT_FAILURE;
    }

    if (ap.args_sz() < 1)
    {
        fprintf(stderr, "specify at least one input file\n");
        return EXIT_FAILURE;
    }

    std::vector<double> percentiles;

    if (!parse_percentiles(pcs_str, &percentiles))
    {
        return EXIT_FAILURE;
    }

    std::vector<series_description> series = compute_series(ap.args(), ap.args_sz());
    assert(series.size() == ap.args_sz());
    printf("# series");

    for (size_t p = 0; p < percentiles.size(); ++p)
    {
        printf("\t%g", percentiles[p]);
    }

    printf("\n");

    for (size_t i = 0; i < series.size(); ++i)
    {
        ygor_data_reader* ydr = NULL;
        ygor_data_iterator* ydi = NULL;
        bool has_space = false;

        for (const char* c = ap.args()[i]; !has_space && *c; ++c)
        {
            has_space = has_space || isspace(*c);
        }

        if (has_space)
        {
            printf("\"%s\"", ap.args()[i]);
        }
        else
        {
            printf("%s", ap.args()[i]);
        }

        if (!(ydr = ygor_data_reader_create(series[i].filename.c_str())) ||
            !(ydi = ygor_data_iterate(ydr, series[i].series_name.c_str())) ||
            !(ydi = ygor_data_convert_units(ydi, ygor_data_iterator_series(ydi)->indep_units, sopts.units())))
        {
            fprintf(stderr, "cannot create iterator from input %s\n", ap.args()[i]);
            return EXIT_FAILURE;
        }

        for (size_t p = 0; p < percentiles.size(); ++p)
        {
            double tile;

            if (ygor_percentile(ydi, percentiles[p]/100., &tile) < 0 ||
                ygor_data_iterator_rewind(ydi) < 0)
            {
                fprintf(stderr, "cannot calculate percentile %g from input %s\n", percentiles[p], ap.args()[i]);
                return EXIT_FAILURE;
            }

            printf("\t%g", tile);
        }

        printf("\n");
        ygor_data_iterator_destroy(ydi);
        ygor_data_reader_destroy(ydr);
    }

    return EXIT_SUCCESS;
}
