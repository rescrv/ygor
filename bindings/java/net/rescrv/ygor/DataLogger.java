/* Copyright (c) 2014, Robert Escriva
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
 *     * Neither the name of Ygor nor the names of its contributors may be
 *       used to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package net.rescrv.ygor;

public class DataLogger
{
    static
    {
        System.loadLibrary("ygor-java");
        initialize();
    }

    private long ptr = 0;

    public DataLogger(String output, long scale_when, long scale_data)
    {
        _create(output, scale_when, scale_data);
    }

    protected void finalize() throws Throwable
    {
        try
        {
            flush_and_destroy();
        }
        finally
        {
            super.finalize();
        }
    }

    public synchronized void flush_and_destroy()
    {
        if (ptr != 0)
        {
            _destroy();
            ptr = 0;
        }
    }

    public void record(long series, long data)
    {
        record(series, System.nanoTime(), data);
    }
    public native void record(long series, long when, long data);

    /* cached IDs */
    private static native void initialize();
    private static native void terminate();
    /* ctor/dtor */
    private native void _create(String output, long scale_when, long scale_data);
    private native void _destroy();
}
