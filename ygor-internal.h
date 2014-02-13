// Copyright (c) 2014, Robert Escriva
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

#ifndef ygor_internal_h_
#define ygor_internal_h_

// e
#include <e/popt.h>

#define YGOR_API __attribute__ ((visibility ("default")))

struct ygor_series
{
    struct ygor_data_point* data;
    uint64_t data_sz;
};

class scale_opts
{
    public:
        scale_opts()
            : m_ap()
            , m_scale_str("auto")
        {
            m_ap.arg().name('s', "scale")
                      .description("Scale (default: auto)")
                      .as_string(&m_scale_str);
        }

    public:
        const e::argparser& parser() { return m_ap; }
        bool scale(double v, uint64_t* s, const char** ss)
        {
            if (strcmp(m_scale_str, "auto") == 0)
            {
                ygor_autoscale(v, ss);
                int x = ygor_unit_scaling_factor(*ss, s);
                assert(x == 0);
                return true;
            }
            else
            {
                *ss = m_scale_str;

                if (ygor_unit_scaling_factor(m_scale_str, s) < 0)
                {
                    fprintf(stderr, "cannot understand scale\n");
                    return false;
                }

                if (*s < 1)
                {
                    fprintf(stderr, "scale must be positive\n");
                    return false;
                }

                return true;
            }
        }

    private:
        scale_opts(const scale_opts&);
        scale_opts& operator = (const scale_opts&);

    private:
        e::argparser m_ap;
        const char* m_scale_str;
};

class bucket_scale_opts
{
    public:
        bucket_scale_opts()
            : m_ap()
            , m_bucket_str("1ms")
            , m_scale_str("auto")
            , m_bucket(1000ULL * 1000ULL)
            , m_scale(1000ULL * 1000ULL)
        {
            m_ap.arg().name('b', "bucket")
                      .description("Bucket size (default: 1ms)")
                      .as_string(&m_bucket_str);
            m_ap.arg().name('s', "scale")
                      .description("Scale (default: auto)")
                      .as_string(&m_scale_str);
        }

    public:
        const e::argparser& parser() { return m_ap; }
        uint64_t bucket() const { return m_bucket; }
        uint64_t scale() const { return m_scale; }
        uint64_t index(uint64_t idx) const { return m_bucket * idx; }
        uint64_t index_scaled(uint64_t idx) const { return m_bucket / m_scale * idx; }
        bool validate()
        {
            if (ygor_bucket_size(m_bucket_str, &m_bucket) < 0)
            {
                fprintf(stderr, "cannot understand bucket size\n");
                return false;
            }

            if (strcmp(m_scale_str, "auto") == 0)
            {
                ygor_autoscale(m_bucket, &m_scale_str);
            }

            if (ygor_unit_scaling_factor(m_scale_str, &m_scale) < 0)
            {
                fprintf(stderr, "cannot understand scale\n");
                return false;
            }

            if (m_scale < 1)
            {
                fprintf(stderr, "scale must be positive\n");
                return false;
            }

            if (ygor_validate_bucket_units(m_bucket, m_scale) < 0)
            {
                fprintf(stderr, "bucket size must be a multiple of scale\n");
                return false;
            }

            return true;
        }

    private:
        bucket_scale_opts(const bucket_scale_opts&);
        bucket_scale_opts& operator = (const bucket_scale_opts&);

    private:
        e::argparser m_ap;
        const char* m_bucket_str;
        const char* m_scale_str;
        uint64_t m_bucket;
        uint64_t m_scale;
};

#endif // ygor_internal_h_
