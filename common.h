// Copyright (c) 2014,2017, Robert Escriva
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

#ifndef ygor_common_h_
#define ygor_common_h_

// e
#include <e/popt.h>

// ygor
#include <ygor/data.h>

const char*
units_to_str(ygor_units u);
bool
str_to_units(const char* suffix, ygor_units* u);

const char*
precision_to_str(ygor_precision p);

struct series_description
{
    series_description();
    series_description(const char* f, const char* s);

    std::string filename;
    std::string series_name;
};

std::vector<series_description>
compute_series(const char** args, size_t args_sz);

struct data_points
{
    data_points();
    data_points(ygor_data_point* data, uint64_t data_sz);

    ygor_data_point* data;
    uint64_t data_sz;
};

class bucket_options
{
    public:
        bucket_options();

    public:
        const e::argparser& parser();
        uint64_t bucket();
        ygor_units units();
        bool validate();

    private:
        bucket_options(const bucket_options&);
        bucket_options& operator = (const bucket_options&);

    private:
        e::argparser m_ap;
        const char* m_bucket_str;
        const char* m_units_str;
        uint64_t m_bucket;
        ygor_units m_bucket_units;
};

#endif // ygor_common_h_
