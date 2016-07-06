# Copyright (c) 2014, Robert Escriva
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
#     * Neither the name of Ygor nor the names of its contributors may be used
#       to endorse or promote products derived from this software without
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
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([ygor], [0.1.dev], [robert@rescrv.net])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-bzip2])
AM_SILENT_RULES([yes])
AM_PATH_PYTHON([2.6])
LT_PREREQ([2.2])
LT_INIT
AC_CONFIG_SRCDIR([ygor.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_LANG(C++)

ANAL_WARNINGS

AC_ARG_ENABLE([java-bindings], [AS_HELP_STRING([--enable-java-bindings],
              [enable the Java bindings @<:@default: no@:>@])],
              [java_bindings=${enableval}], [java_bindings=no])

JNI_CPPFLAGS=
if test x"${java_bindings}" = xyes; then
    AC_PROG_JAR
    AC_PROG_JAVAC
    AC_JNI_INCLUDE_DIR
    found_jnidir=no
    for jnidir in ${JNI_INCLUDE_DIRS}
    do
        if test x"${found_jnidir}" = xno; then
            JNI_CPPFLAGS="-I${jnidir} -I${jnidir}/linux"
            found_jnidir=yes
        fi
    done
    if test x"${found_jnidir}" = xno; then
        AC_MSG_ERROR([
-------------------------------------------------
Ygor relies upon the JNI for Java bindings.
No JNI include dir could be found.
Set JAVAC to point to your Java compiler.
-------------------------------------------------])
    fi
fi
AC_SUBST([JNI_CPPFLAGS], [${JNI_CPPFLAGS}])

# Checks for libraries.
AC_CHECK_LIB([popt], [poptGetContext], [], [AC_MSG_ERROR([
------------------------------------
ygor relies upon the popt library.
Please install popt to continue.
------------------------------------])])
AC_CHECK_LIB([rt], [clock_gettime], [needs_lrt=yes], [needs_lrt=no])

if test x"${needs_lrt}" = xyes; then
    AC_SUBST([LRT_LDFLAGS], ["-lrt"])
else
    AC_SUBST([LRT_LDFLAGS], [""])
fi

PKG_CHECK_MODULES([PO6], [libpo6 >= 0.3.1])
PKG_CHECK_MODULES([E], [libe >= 0.3.2])

AC_ARG_ENABLE([python_bindings], [AS_HELP_STRING([--enable-python-bindings],
              [build Python bindings @<:@default: no@:>@])],
              [python_bindings=${enableval}], [python_bindings=no])
if test x"${python_bindings}" = xyes; then
    AC_PYTHON_DEVEL([>= '2.6'])
    AS_CASE([$PYTHON_VERSION], [3*], [pythonsym=PyInit_], [2*], [pythonsym=init], [])
    AC_SUBST([PYTHON_SYMBOL], [${pythonsym}])
fi
AM_CONDITIONAL([ENABLE_PYTHON_BINDINGS], [test x"${python_bindings}" = xyes])

# Checks for header files.
AC_CHECK_HEADER([popt.h],,[AC_MSG_ERROR([
------------------------------------
ygor relies upon the popt library.
Please install popt to continue.
------------------------------------])])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime mach_absolute_time])

AM_CONDITIONAL([ENABLE_JAVA_BINDINGS], [test x"${java_bindings}" = xyes])

AC_CONFIG_FILES([Makefile
                 ygor.pc])
AC_OUTPUT
