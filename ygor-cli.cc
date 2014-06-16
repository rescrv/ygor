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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// STL
#include <vector>

// e
#include <e/subcommand.h>

int
main(int argc, const char* argv[])
{
    std::vector<e::subcommand> cmds;
    cmds.push_back(e::subcommand("armnod",      "Generate random strings"));
    cmds.push_back(e::subcommand("configure",   "Configure an experiment for Ygor"));
    cmds.push_back(e::subcommand("run",         "Have Ygor run an experiment"));
    cmds.push_back(e::subcommand("cdf",         "Generate a CDF of the data"));
    cmds.push_back(e::subcommand("dump",        "Dump the raw data"));
    cmds.push_back(e::subcommand("merge",       "Merge multiple data files"));
    cmds.push_back(e::subcommand("summarize",   "Generate a summary of the data"));
    cmds.push_back(e::subcommand("timeseries",  "Generate a timeseries of the data"));
    cmds.push_back(e::subcommand("t-test",      "Run the Student's t-test on multiple data files"));
    cmds.push_back(e::subcommand("graph",       "Graph gnuplot files with Jinaj2 preprocessing"));
    return dispatch_to_subcommands(argc, argv,
                                   "ygor", "ygor",
                                   PACKAGE_VERSION,
                                   "ygor-",
                                   "YGOR_EXEC_PATH", YGOR_EXEC_DIR,
                                   &cmds.front(), cmds.size());
}
