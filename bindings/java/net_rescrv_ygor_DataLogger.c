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

/* C */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* POSIX */
#include <errno.h>
#include <time.h>

/* Ygor */
#include <ygor.h>
#include "visibility.h"
#include "bindings/java/net_rescrv_ygor_DataLogger.h"

/********************************* Cached IDs *********************************/

static jclass _dl;
static jfieldID _dl_ptr;

#define CHECK_CACHE(X) assert((X))

#define ERROR_CHECK(RET) if ((*env)->ExceptionCheck(env) == JNI_TRUE) return (RET)
#define ERROR_CHECK_VOID() if ((*env)->ExceptionCheck(env) == JNI_TRUE) return

#define REF(NAME, DEF) \
    tmp_cls = (DEF); \
    NAME = (jclass) (*env)->NewGlobalRef(env, tmp_cls); \
    (*env)->DeleteLocalRef(env, tmp_cls);

JNIEXPORT YGOR_API void JNICALL
Java_net_rescrv_ygor_DataLogger_initialize(JNIEnv* env, jclass datalogger)
{
    jclass tmp_cls;

    REF(_dl, (*env)->FindClass(env, "net/rescrv/ygor/DataLogger"));
    _dl_ptr = (*env)->GetFieldID(env, _dl, "ptr", "J");

    CHECK_CACHE(_dl);
    CHECK_CACHE(_dl_ptr);
    (void)datalogger;
}

JNIEXPORT YGOR_API void JNICALL
Java_net_rescrv_ygor_DataLogger_terminate(JNIEnv* env, jclass datalogger)
{
    (*env)->DeleteGlobalRef(env, _dl);
    (void)datalogger;
}

/******************************* Pointer Unwrap *******************************/

static struct ygor_data_logger*
ygor_get_ptr(JNIEnv* env, jobject obj)
{
    struct ygor_data_logger* dl;
    dl = (struct ygor_data_logger*) (*env)->GetLongField(env, obj, _dl_ptr);
    assert(dl);
    return dl;
}

/******************************** Client Class ********************************/

JNIEXPORT YGOR_API void JNICALL
Java_net_rescrv_ygor_DataLogger__1create(JNIEnv* env, jobject jdl, jstring jout, jlong scale_when, jlong scale_data)
{
    jlong lptr;
    const char* out;
    struct ygor_data_logger* ptr;

    assert(scale_when >= 1);
    assert(scale_data >= 1);

    lptr = (*env)->GetLongField(env, jdl, _dl_ptr);
    ERROR_CHECK_VOID();
    out = (*env)->GetStringUTFChars(env, jout, NULL);
    ERROR_CHECK_VOID();
    ptr = ygor_data_logger_create(out, scale_when, scale_data);
    (*env)->ReleaseStringUTFChars(env, jout, out);

    if (!ptr)
    {
        /* XXX */
        printf("poor man's exception:  could not open output: %s\n", strerror(errno));
        abort();
    }

    ERROR_CHECK_VOID();
    lptr = (long) ptr;
    (*env)->SetLongField(env, jdl, _dl_ptr, lptr);
    ERROR_CHECK_VOID();
    assert(sizeof(long) >= sizeof(struct hyperdex_client*));
}

JNIEXPORT YGOR_API void JNICALL
Java_net_rescrv_ygor_DataLogger__1destroy(JNIEnv* env, jobject jdl)
{
    jlong lptr;
    struct ygor_data_logger* ptr;

    lptr = (*env)->GetLongField(env, jdl, _dl_ptr);
    ERROR_CHECK_VOID();
    ptr = (struct ygor_data_logger*)lptr;

    if (ptr)
    {
        if (ygor_data_logger_flush_and_destroy(ptr) < 0)
        {
            /* XXX */
            printf("poor man's exception:  could not flush output: %s\n", strerror(errno));
            abort();
        }
    }

    (*env)->SetLongField(env, jdl, _dl_ptr, 0);
    ERROR_CHECK_VOID();
}

JNIEXPORT YGOR_API void JNICALL
Java_net_rescrv_ygor_DataLogger_record(JNIEnv* env, jobject jdl, jlong series, jlong when, jlong data)
{
    struct ygor_data_record ydr;

    assert(series >= 0);
    assert(when >= 0);
    assert(data >= 0);
    ydr.series = series;
    ydr.when = when;
    ydr.data = data;

    if (ygor_data_logger_record(ygor_get_ptr(env, jdl), &ydr) < 0)
    {
        /* XXX */
        printf("poor man's exception:  could not flush output: %s\n", strerror(errno));
        abort();
    }
}

JNIEXPORT YGOR_API jlong JNICALL
Java_net_rescrv_ygor_DataLogger_time(JNIEnv* env, jclass jdl)
{
    uint64_t now;
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
    {
        abort();
    }

    now =  ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    (void)env;
    (void)jdl;
    return (jlong)now;
}
