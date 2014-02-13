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

from cpython cimport bool

cdef extern from "stdint.h":

    ctypedef unsigned int uint32_t
    ctypedef long unsigned int uint64_t

cdef extern from "ygor.h":

    cdef struct ygor_data_record:
        uint32_t flags
        uint64_t when
        uint64_t data

    cdef struct ygor_data_logger
    ygor_data_logger* ygor_data_logger_create(char* output,
                                              uint64_t scale_when,
                                              uint64_t scale_data)
    int ygor_data_logger_flush_and_destroy(ygor_data_logger* ydl)

    int ygor_data_logger_start(ygor_data_logger* ydl, ygor_data_record* ydp)
    int ygor_data_logger_finish(ygor_data_logger* ydl, ygor_data_record* ydp)
    int ygor_data_logger_record(ygor_data_logger* ydl, ygor_data_record* ydp)

cdef class DataLogger:

    cdef ygor_data_logger* dl

    def __cinit__(self, bytes output, long scale_when, long scale_data):
        if scale_when < 1:
            raise RuntimeError("scale_when must be > 0")
        if scale_data < 1:
            raise RuntimeError("scale_data must be > 0")
        self.dl = ygor_data_logger_create(output, scale_when, scale_data)
        if not self.dl:
            raise RuntimeError("error creating data logger")

    def __dealloc__(self):
        if self.dl:
            ygor_data_logger_flush_and_destroy(self.dl)
            self.dl = NULL

    def record(self, long flags, long when, long data):
        cdef ygor_data_record ydr
        ydr.flags = flags;
        ydr.when = when;
        ydr.data = data;
        if ygor_data_logger_record(self.dl, &ydr) < 0:
            raise RuntimeError("could not log data record")
