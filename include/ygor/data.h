/* Copyright (c) 2013-2017, Robert Escriva
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of ygor nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ygor_data_h_
#define ygor_data_h_

/* C */
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

enum ygor_units
{
    YGOR_UNIT_S   = 1,
    YGOR_UNIT_MS  = 2,
    YGOR_UNIT_US  = 3,
    YGOR_UNIT_BYTES    = 9,
    YGOR_UNIT_KBYTES   = 10,
    YGOR_UNIT_MBYTES   = 11,
    YGOR_UNIT_GBYTES   = 12,
    YGOR_UNIT_MONOTONIC= 254,
    YGOR_UNIT_UNIT     = 255
};

enum ygor_precision
{
    YGOR_PRECISE_INTEGER    = 1,
    YGOR_HALF_PRECISION     = 2,
    YGOR_SINGLE_PRECISION   = 3,
    YGOR_DOUBLE_PRECISION   = 4
};

struct ygor_series
{
    const char* name;
    enum ygor_units indep_units;
    enum ygor_precision indep_precision;
    enum ygor_units dep_units;
    enum ygor_precision dep_precision;
};

union ygor_data_value
{
    uint64_t precise;
    double approximate;
};

struct ygor_data_point
{
    /* must point to a pointer passed to create */
    const struct ygor_series* series;
    /* precise or approximate will be used depending on precision */
    union ygor_data_value indep;
    union ygor_data_value dep;
};

struct ygor_data_logger;
struct ygor_data_logger* ygor_data_logger_create(const char* output,
                                                 const struct ygor_series** series,
                                                 size_t series_sz);
int ygor_data_logger_flush_and_destroy(struct ygor_data_logger* ydl);
int ygor_data_logger_record(struct ygor_data_logger* ydl, struct ygor_data_point* ydp);

struct ygor_data_reader;
struct ygor_data_reader* ygor_data_reader_create(const char* input);
void ygor_data_reader_destroy(struct ygor_data_reader* ydr);
size_t ygor_data_reader_num_series(struct ygor_data_reader* ydr);
const struct ygor_series* ygor_data_reader_series(struct ygor_data_reader* ydr, size_t idx);

struct ygor_data_iterator;
struct ygor_data_iterator* ygor_data_iterate(struct ygor_data_reader* ydr, const char* name);
void ygor_data_iterator_destroy(struct ygor_data_iterator* ydi);
const struct ygor_series* ygor_data_iterator_series(struct ygor_data_iterator* ydi);
int ygor_data_iterator_valid(struct ygor_data_iterator* ydi);
void ygor_data_iterator_advance(struct ygor_data_iterator* ydi);
void ygor_data_iterator_read(struct ygor_data_iterator* ydi,
                             struct ygor_data_point* ydp);
struct ygor_data_iterator* ygor_data_convert_units(struct ygor_data_iterator* ydi,
                                                   enum ygor_units new_indep_units,
                                                   enum ygor_units new_dep_units);

int ygor_cdf(struct ygor_data_iterator* ydi, uint64_t step_value,
             struct ygor_data_point** data, uint64_t* data_sz);
int ygor_timeseries(struct ygor_data_iterator* ydi, uint64_t step_value,
                    struct ygor_data_point** data, uint64_t* data_sz);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* ygor_data_h_ */
