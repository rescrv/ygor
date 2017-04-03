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
#include <signal.h>
#include <stdlib.h>

// STL
#include <memory>

// e
#include <e/popt.h>

// ygor
#include <ygor/armnod.h>

int
main(int argc, const char* argv[])
{
    e::argparser ap;
    std::auto_ptr<armnod_argparser> apl(armnod_argparser::create(""));
    ap.autohelp();
    ap.add("Generator:", apl->parser());

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

    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGPROF);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    if (!apl->config())
    {
        ap.usage();
        return EXIT_FAILURE;
    }

    armnod_generator* gen = armnod_generator_create(apl->config());

    for (uint64_t i = 0; ; ++i)
    {
        if ((i & 0xf) == 0)
        {
            sigpending(&mask);

            if (sigismember(&mask, SIGHUP) ||
                sigismember(&mask, SIGINT) ||
                sigismember(&mask, SIGTERM) ||
                sigismember(&mask, SIGPIPE) ||
                sigismember(&mask, SIGKILL))
            {
                break;
            }
        }

        const char* string = armnod_generate(gen);

        if (!string)
        {
            break;
        }

        printf("%s\n", string);
    }

    armnod_generator_destroy(gen);
    return EXIT_SUCCESS;
}
