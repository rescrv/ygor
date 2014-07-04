/* Copyright (c) 2013-2014, Robert Escriva
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
 *     * Neither the name of Ygor nor the names of its contributors may be used
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

#ifndef ygor_h_
#define ygor_h_
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* C */
#include <stdint.h>
#include <stdio.h>

/* POSIX */
#include <sys/resource.h>

/* data */
struct ygor_data_record
{
    uint32_t series;
    uint64_t when;
    uint64_t data;
};

/* data logger */
struct ygor_data_logger;
struct ygor_data_logger* ygor_data_logger_create(const char* output,
                                                 uint64_t scale_when,
                                                 uint64_t scale_data);
int ygor_data_logger_flush_and_destroy(struct ygor_data_logger* ydl);
int ygor_data_logger_start(struct ygor_data_logger* ydl, struct ygor_data_record* ydp);
int ygor_data_logger_finish(struct ygor_data_logger* ydl, struct ygor_data_record* ydp);
int ygor_data_logger_record(struct ygor_data_logger* ydl, struct ygor_data_record* ydp);

/* data iterator */
struct ygor_data_iterator;
struct ygor_data_iterator* ygor_data_iterator_create(const char* input);
void ygor_data_iterator_destroy(struct ygor_data_iterator* ydi);
uint64_t ygor_data_iterator_when_scale(struct ygor_data_iterator* ydi);
uint64_t ygor_data_iterator_data_scale(struct ygor_data_iterator* ydi);

int ygor_data_iterator_valid(struct ygor_data_iterator* ydi);
void ygor_data_iterator_advance(struct ygor_data_iterator* ydi);
void ygor_data_iterator_read(struct ygor_data_iterator* ydi,
                             struct ygor_data_record* ydp);

/* scaling values */
/* store in *nanos the number of nanoseconds in each unit.
 * e.g., ns = 1, us = 1,000, ms = 1,000,000, etc.
 */
int ygor_unit_scaling_factor(const char* unit, uint64_t* nanos);
/* store in *nanos the number of nanoseconds in each unit.
 * e.g., 10us = 10,000, 10ms = 10,000,000
 */
int ygor_bucket_size(const char* bucket, uint64_t* nanos);
/* check that the bucket size is a multiple of units */
int ygor_validate_bucket_units(uint64_t bucket_nanos, uint64_t unit_nanos);
/* find the largest unit such that value spans more nanoseconds than one unit */
void ygor_autoscale(double value, const char** str);

/* collect getrusage stats */
int ygor_data_logger_record_rusage_stats(struct ygor_data_logger* ydl, uint64_t when, struct rusage* usage);

/* collect system io stats (Linux only) */
struct ygor_io_stats
{
    uint64_t read_ios;
    uint64_t read_merges;
    uint64_t read_bytes;
    uint64_t read_ticks;
    uint64_t write_ios;
    uint64_t write_merges;
    uint64_t write_bytes;
    uint64_t write_ticks;
    uint64_t in_flight;
    uint64_t io_ticks;
    uint64_t time_in_queue;
};
int ygor_io_block_stat_path(const char* path, char* stat_path, size_t stat_path_sz);
int ygor_io_collect_stats(char* path, struct ygor_io_stats* stats);
int ygor_data_logger_record_io_stats(struct ygor_data_logger* ydl, uint64_t when, const struct ygor_io_stats* stats);

/* predefined series
 * Ygor reserves series with bits 0xffff0000U set for itself.  They are used to
 * collect system stats and the like that Ygor supports out-of-the-box.
 */
#define SERIES_RU_UTIME             0xffff0000U
#define SERIES_RU_STIME             0xffff0001U
#define SERIES_RU_MAXRSS            0xffff0002U
#define SERIES_RU_IXRSS             0xffff0003U
#define SERIES_RU_IDRSS             0xffff0004U
#define SERIES_RU_ISRSS             0xffff0005U
#define SERIES_RU_MINFLT            0xffff0006U
#define SERIES_RU_MAJFLT            0xffff0007U
#define SERIES_RU_NSWAP             0xffff0008U
#define SERIES_RU_INBLOCK           0xffff0009U
#define SERIES_RU_OUBLOCK           0xffff000aU
#define SERIES_RU_MSGSND            0xffff000bU
#define SERIES_RU_MSGRCV            0xffff000cU
#define SERIES_RU_NSIGNALS          0xffff000dU
#define SERIES_RU_NVCSW             0xffff000eU
#define SERIES_RU_NIVCSW            0xffff000fU

#define SERIES_IO_READ_IOS          0xffff0010U
#define SERIES_IO_READ_MERGES       0xffff0011U
#define SERIES_IO_READ_BYTES        0xffff0012U
#define SERIES_IO_READ_TICKS        0xffff0013U
#define SERIES_IO_WRITE_IOS         0xffff0014U
#define SERIES_IO_WRITE_MERGES      0xffff0015U
#define SERIES_IO_WRITE_BYTES       0xffff0016U
#define SERIES_IO_WRITE_TICKS       0xffff0017U
#define SERIES_IO_IN_FLIGHT         0xffff0018U
#define SERIES_IO_IO_TICKS          0xffff0019U
#define SERIES_IO_TIME_IN_QUEUE     0xffff001aU

/* data analysis */
struct ygor_data_point
{
    uint64_t x;
    double y;
};

struct ygor_summary
{
    uint64_t points;
    uint64_t nanos;
    double mean;
    double stdev;
    double variance;
};

struct ygor_difference
{
    double raw;
    double raw_plus_minus;
    double percent;
    double percent_plus_minus;
};

int ygor_cdf(const char* input, uint64_t bucket_nanos,
             struct ygor_data_point** data, uint64_t* data_sz);
int ygor_timeseries(const char* input, uint64_t bucket_nanos,
                    struct ygor_data_point** data, uint64_t* data_sz);
int ygor_summarize(const char* input, struct ygor_summary* summary);
int ygor_t_test(struct ygor_summary* baseline,
                struct ygor_summary* compared,
                double interval,
                struct ygor_difference* diff);

/* random strings */

struct armnod_config;
struct armnod_config* armnod_config_create();
struct armnod_config* armnod_config_copy(struct armnod_config*);
void armnod_config_destroy(struct armnod_config* ac);

/* A c-string containing the alphabet of the random string.
 * Repeated characters will bias the results accordingly.
 */
int armnod_config_alphabet(struct armnod_config* ac, const char* chars);
/* Set the alphabet to one of several presets:
 * default: alnum | punct | " "
 * alnum:   [a-zA-Z0-9]
 * alpha:   [a-zA-Z]
 * digit:   [0-9]
 * lower:   [a-z]
 * upper:   [A-Z]
 * punct:   "!\"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~"
 * hex:     [0-9a-f]
 */
int armnod_config_charset(struct armnod_config* ac, const char* name);

/* Generate random strings that are unlikely to repeat.
 * This is the default behavior.
 */
int armnod_config_choose_default(struct armnod_config* ac);
/* Generate random strings from a fixed size set.
 * The strings will be selected from the fixed-size set uniformly at random, and
 * will repeat indefinitely.
 */
int armnod_config_choose_fixed(struct armnod_config* ac, uint64_t size);
/* Generate random strings from a fixed size set.
 * Each string in the set will be generated exactly once.
 */
int armnod_config_choose_fixed_once(struct armnod_config* ac, uint64_t size);
/* Generate random strings from a fixed size set.
 * The strings will be selected from the fixed-size set at random according to a
 * Zipf distribution with parameter "alpha", and will repeat indefinitely.
 */
int armnod_config_choose_fixed_zipf(struct armnod_config* ac,
                                    uint64_t size, double alpha);

/* Generate strings of a constant length */
int armnod_config_length_constant(struct armnod_config* ac, uint64_t length);
/* Generate strings of length uniformly distributed between min, max */
int armnod_config_length_uniform(struct armnod_config* ac,
                                 uint64_t min, uint64_t max);

struct armnod_generator;
struct armnod_generator* armnod_generator_create(const struct armnod_config* ac);
void armnod_generator_destroy(struct armnod_generator* ag);

/* Seed the random portion used to select strings */
void armnod_seed(struct armnod_generator* ag, uint64_t seed);
/* Generate a random string using the provided generator.
 * The string will be constructed according to the generator's configuration and
 * the next string in the sequence will be returned.  The returned string points
 * to internal storage and should not be modified or freed.
 *
 * If the generator returns NULL, it means that there are no more strings to
 * return.
 */
const char* armnod_generate(struct armnod_generator* ag);
/* Same as "armnod_generate", but returns the size of the string to avoid a
 * subsequent call to "strlen".
 */
const char* armnod_generate_sz(struct armnod_generator* ag, uint64_t* sz);
/* Generate the specified string from the set */
const char* armnod_generate_idx(struct armnod_generator* ag, uint64_t idx);
const char* armnod_generate_idx_sz(struct armnod_generator* ag, uint64_t idx, uint64_t* sz);
/* Return the member of the set that would be generated (always 0 if not
 * supported by string generation method, e.g. non-fixed sets)
 */
uint64_t armnod_generate_idx_only(struct armnod_generator* ag);

#ifdef __cplusplus
} /* extern "C" */

// e
#include <e/popt.h>

struct armnod_argparser
{
    static armnod_argparser* create(const char* prefix, bool method=true);
    virtual ~armnod_argparser() throw () = 0;
    virtual const e::argparser& parser() = 0;
    virtual armnod_config* config() = 0;
};

#endif /* __cplusplus */
#endif // ygor_h_
