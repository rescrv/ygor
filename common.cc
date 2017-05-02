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

// POSIX
#include <err.h>
#include <sys/stat.h>

// ygor
#include "common.h"
#include "ygor-internal.h"

const char*
units_to_str(ygor_units u)
{
    switch (u)
    {
        case YGOR_UNIT_S: return "s";
        case YGOR_UNIT_MS: return "ms";
        case YGOR_UNIT_US: return "us";
        case YGOR_UNIT_BYTES: return "B";
        case YGOR_UNIT_KBYTES: return "KB";
        case YGOR_UNIT_MBYTES: return "MB";
        case YGOR_UNIT_GBYTES: return "GB";
        case YGOR_UNIT_MONOTONIC: return "monotonic";
        case YGOR_UNIT_UNIT: return "unit";
        default: return "unknown";
    }
}

bool
str_to_units(const char* _suffix, ygor_units* u)
{
    std::string suffix(_suffix);

    if (suffix == "s")
    {
        *u = YGOR_UNIT_S;
    }
    else if (suffix == "ms")
    {
        *u = YGOR_UNIT_MS;
    }
    else if (suffix == "us")
    {
        *u = YGOR_UNIT_US;
    }
    else if (suffix == "B")
    {
        *u = YGOR_UNIT_BYTES;
    }
    else if (suffix == "KB")
    {
        *u = YGOR_UNIT_KBYTES;
    }
    else if (suffix == "MB")
    {
        *u = YGOR_UNIT_MBYTES;
    }
    else if (suffix == "GB")
    {
        *u = YGOR_UNIT_GBYTES;
    }
    else if (suffix == "monotonic")
    {
        *u = YGOR_UNIT_MONOTONIC;
    }
    else if (suffix == "unit")
    {
        *u = YGOR_UNIT_UNIT;
    }
    else
    {
        return false;
    }

    return true;
}

const char*
precision_to_str(ygor_precision p)
{
    switch (p)
    {
        case YGOR_PRECISE_INTEGER: return "precise";
        case YGOR_HALF_PRECISION: return "half-precision";
        case YGOR_SINGLE_PRECISION: return "single-precision";
        case YGOR_DOUBLE_PRECISION: return "double-precision";
        default: return "unknown";
    }
}

series_description :: series_description()
    : filename()
    , series_name()
{
}

series_description :: series_description(const char* f, const char* s)
    : filename(f)
    , series_name(s)
{
}

std::vector<series_description>
compute_series(const char** args, size_t args_sz)
{
    std::vector<series_description> sds;

    for (size_t i = 0; i < args_sz; ++i)
    {
        struct stat st;
        int ret = stat(args[i], &st);

        if (ret < 0 && errno == ENOENT)
        {
            const char* sep = strrchr(args[i], ':');

            if (!sep)
            {
                err(EXIT_FAILURE, "could not process %s", args[i]);
            }

            std::string tmp(args[i], sep);

            if (stat(tmp.c_str(), &st) < 0)
            {
                err(EXIT_FAILURE, "could not process %s", args[i]);
            }

            sds.push_back(series_description(tmp.c_str(), sep + 1));
        }
        else if (ret < 0)
        {
            err(EXIT_FAILURE, "could not process %s", args[i]);
        }
        else
        {
            sds.push_back(series_description(args[i], "*"));
        }
    }

    return sds;
}

data_points :: data_points()
    : data(NULL)
    , data_sz(0)
{
}

data_points :: data_points(ygor_data_point* _data, uint64_t _data_sz)
    : data(_data)
    , data_sz(_data_sz)
{
}

scale_options :: scale_options()
    : m_ap()
    , m_units_str("ms")
    , m_units()
{
    m_ap.arg().name('u', "units")
              .description("Units (default: ms)")
              .as_string(&m_units_str);
}

const e::argparser&
scale_options :: parser()
{
    return m_ap;
}

ygor_units
scale_options :: units()
{
    bool valid = validate();
    assert(valid);
    return m_units;
}

bool
scale_options :: validate()
{
    return str_to_units(m_units_str, &m_units);
}

bucket_options :: bucket_options()
    : m_ap()
    , m_bucket_str("1ms")
    , m_units_str("auto")
    , m_bucket()
    , m_bucket_units()
{
    m_ap.arg().name('b', "bucket")
              .description("Bucket size (default: 1ms)")
              .as_string(&m_bucket_str);
    m_ap.arg().name('u', "units")
              .description("Units (default: auto)")
              .as_string(&m_units_str);
}

const e::argparser&
bucket_options :: parser()
{
    return m_ap;
}

uint64_t
bucket_options :: bucket()
{
    bool valid = validate();
    assert(valid);
    return m_bucket;
}

ygor_units
bucket_options :: units()
{
    bool valid = validate();
    assert(valid);
    return m_bucket_units;
}

bool
bucket_options :: validate()
{
    char* end = NULL;
    m_bucket = strtoull(m_bucket_str, &end, 10);

    if (*end == '\0')
    {
        m_bucket_units = YGOR_UNIT_UNIT;
    }
    else if (!str_to_units(end, &m_bucket_units))
    {
        return false;
    }

    ygor_units u = m_bucket_units;

    if (strcmp(m_units_str, "auto") != 0 && !str_to_units(m_units_str, &u))
    {
        return false;
    }

    if (!ygor_units_compatible(m_bucket_units, u))
    {
        return false;
    }

    double ratio = ygor_units_conversion_ratio(m_bucket_units, u);

    if (ratio < 1)
    {
        return false;
    }

    m_bucket_units = u;
    m_bucket *= ratio;
    return m_bucket > 0;
}
