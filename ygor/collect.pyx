# Copyright (c) 2013, Robert Escriva
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of this project nor the names of its contributors may
#       used to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import collections

from cpython cimport bool

cdef extern from "stdlib.h":

    void* malloc(size_t sz)
    void free(void* ptr)

cdef extern from "stdint.h":

    ctypedef unsigned int uint32_t
    ctypedef long unsigned int uint64_t

cdef extern from "ygor/data.h":

    cdef enum ygor_units:
        YGOR_UNIT_S   = 1
        YGOR_UNIT_MS  = 2
        YGOR_UNIT_US  = 3
        YGOR_UNIT_BYTES    = 9
        YGOR_UNIT_KBYTES   = 10
        YGOR_UNIT_MBYTES   = 11
        YGOR_UNIT_GBYTES   = 12
        YGOR_UNIT_MONOTONIC= 254
        YGOR_UNIT_UNIT     = 255

    cdef enum ygor_precision:
        YGOR_PRECISE_INTEGER    = 1
        YGOR_HALF_PRECISION     = 2
        YGOR_SINGLE_PRECISION   = 3
        YGOR_DOUBLE_PRECISION   = 4

    cdef struct ygor_series:
        const char* name
        ygor_units indep_units
        ygor_precision indep_precision
        ygor_units dep_units
        ygor_precision dep_precision

    cdef union ygor_data_value:
        uint64_t precise
        double approximate

    cdef struct ygor_data_point:
        const ygor_series* series
        ygor_data_value indep
        ygor_data_value dep

    cdef struct ygor_data_logger
    ygor_data_logger* ygor_data_logger_create(const char* output,
                                              const ygor_series** series,
                                              size_t series_sz)
    int ygor_data_logger_flush_and_destroy(ygor_data_logger* ydl)
    int ygor_data_logger_record(ygor_data_logger* ydl, ygor_data_point* ydp)

cdef extern from "ygor-internal.h":
    int ygor_is_precise(ygor_precision p)

Series = collections.namedtuple('Series', ('name', 'indep_units', 'indep_precision', 'dep_units', 'dep_precision'))

units_conversion = {'s': YGOR_UNIT_S,
                    'ms': YGOR_UNIT_MS,
                    'us': YGOR_UNIT_US,
                    'B': YGOR_UNIT_BYTES,
                    'KB': YGOR_UNIT_KBYTES,
                    'MB': YGOR_UNIT_MBYTES,
                    'GB': YGOR_UNIT_GBYTES,
                    None: YGOR_UNIT_UNIT}

precision_conversion = {'precise': YGOR_PRECISE_INTEGER,
                        'half': YGOR_HALF_PRECISION,
                        'single': YGOR_SINGLE_PRECISION,
                        'double': YGOR_DOUBLE_PRECISION}

cdef class __series:

    cdef bytes name
    cdef ygor_series series

cdef class DataLogger:

    cdef ygor_data_logger* dl
    cdef dict series
    cdef dict series_idxs
    cdef const ygor_series** ys;
    cdef size_t ys_sz

    def __cinit__(self, str output, series):
        self.dl = NULL
        self.series = {}
        self.series_idxs = {}
        self.ys = <const ygor_series**>malloc(sizeof(const ygor_series*) * len(series))
        self.ys_sz = len(series)
        for idx, ser in enumerate(series):
            if ser.indep_units not in units_conversion.keys():
                raise ValueError("invalid independent units")
            if ser.indep_precision not in ('half', 'single', 'double', 'precise'):
                raise ValueError("invalid independent precision")
            if ser.dep_units not in units_conversion.keys():
                raise ValueError("invalid dependent units")
            if ser.dep_precision not in ('half', 'single', 'double', 'precise'):
                raise ValueError("invalid dependent precision")
            if ser.name in self.series:
                raise KeyError("series defined twice")
            s = __series()
            s.name = ser.name.encode('utf8')
            s.series.name = s.name
            s.series.indep_units = units_conversion[ser.indep_units]
            s.series.indep_precision = precision_conversion[ser.indep_precision]
            s.series.dep_units = units_conversion[ser.dep_units]
            s.series.dep_precision = precision_conversion[ser.dep_precision]
            self.series[s.name] = s
            self.series_idxs[s.name] = idx
            self.ys[idx] = &s.series
        out = output.encode('utf8')
        self.dl = ygor_data_logger_create(out, self.ys, self.ys_sz)
        if not self.dl:
            raise RuntimeError("error creating data logger")

    def __dealloc__(self):
        if self.dl:
            self.flush_and_destroy()
        if self.ys:
            free(self.ys)

    def flush_and_destroy(self):
        assert(self.dl)
        ygor_data_logger_flush_and_destroy(self.dl)
        self.dl = NULL

    def record(self, str _series, indep, dep):
        cdef bytes series = _series.encode('utf8')
        assert series in self.series
        idx = self.series_idxs[series]
        cdef ygor_data_point ydp
        ydp.series = self.ys[idx]
        if ygor_is_precise(ydp.series.indep_precision):
            ydp.indep.precise = indep
        else:
            ydp.indep.approximate = indep
        if ygor_is_precise(ydp.series.dep_precision):
            ydp.dep.precise = dep
        else:
            ydp.dep.approximate = dep
        if ygor_data_logger_record(self.dl, &ydp) < 0:
            raise RuntimeError("could not log data record")
