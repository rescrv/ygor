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
//     * Neither the name of Ygor nor the names of its contributors may be
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

// STL
#include <memory>

// e
#include <e/popt.h>

// armnod
#include "ygor.h"

int
main(int argc, const char* argv[])
{
    const char* method = "normal";
    bool set_size_trip = false;
    long set_size = 1024;

    e::argparser apg;
    std::auto_ptr<armnod_argparser> apl(armnod_argparser::create(""));
    apg.autohelp();
    apg.arg().long_name("method")
             .description("method to use for generating strings (default: normal)")
             .metavar("METHOD")
             .as_string(&method);
    apg.arg().long_name("set-size")
             .description("number of strings to generate when using --method=fixed")
             .metavar("NUM")
             .as_long(&set_size).set_true(&set_size_trip);
    apg.add("Generator:", apl->parser());

    if (!apg.parse(argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (apg.args_sz() > 0)
    {
        apg.usage();
        std::cerr << "\nerror:  command takes no positional arguments" << std::endl;
        return EXIT_FAILURE;
    }

    if (armnod_config_method(apl->config(), method) < 0)
    {
        std::cerr << "error:  could not set config method" << std::endl;
        return EXIT_FAILURE;
    }

    if (set_size_trip && armnod_config_set_size(apl->config(), set_size) < 0)
    {
        std::cerr << "error:  could not set size" << std::endl;
        return EXIT_FAILURE;
    }

    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    armnod_generator* gen(armnod_generator_create(apl->config()));
    armnod_generator_seed(gen, 0);

    while (true)
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

        std::cout << armnod_generate(gen) << "\n";
    }

    std::cout << std::flush;
    armnod_generator_destroy(gen);
    return EXIT_SUCCESS;
}
