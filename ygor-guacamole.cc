// Copyright (c) 2016, Robert Escriva
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
//     * Neither the name of ygor nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
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
#include <stdlib.h>

// POSIX
#include <unistd.h>

// e
#include <e/popt.h>

// ygor
#include <ygor/guacamole.h>

#define BUF_SZ 262144

int
main(int argc, const char* argv[])
{
    long benchmark = 0;
    long seed = 0;
    e::argparser ap;
    ap.arg().name('b', "benchmark")
            .description("run in benchmark mode (default: no)")
            .as_long(&benchmark);
    ap.arg().name('s', "seed")
            .description("value to seed the rng (default: 0)")
            .as_long(&seed);
    ap.autohelp();

    if (!ap.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (ap.args_sz() > 0)
    {
        ap.usage();
        std::cerr << "\nerror:  command takes no positional arguments" << std::endl;
        return EXIT_FAILURE;
    }

    guacamole* g = guacamole_create(seed);
    char buf[BUF_SZ];

    if (benchmark > 0)
    {
        while (benchmark > 0)
        {
            const size_t gen = benchmark < BUF_SZ ? benchmark : BUF_SZ;
            guacamole_generate(g, buf, gen);
            benchmark -= gen;
        }
    }
    else
    {
        while (true)
        {
            guacamole_generate(g, buf, BUF_SZ);

            if (write(STDOUT_FILENO, buf, BUF_SZ) < 0)
            {
                break;
            }
        }
    }

    return EXIT_SUCCESS;
}
