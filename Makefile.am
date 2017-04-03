# Copyright (c) 2014-2017, Robert Escriva
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

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -I${abs_top_srcdir}/include $(PO6_CFLAGS) $(E_CFLAGS)
AM_CFLAGS   = -fvisibility=hidden $(PO6_CFLAGS) $(E_CFLAGS) $(WANAL_CFLAGS)
AM_CXXFLAGS = -fvisibility=hidden -fvisibility-inlines-hidden $(PO6_CFLAGS) $(E_CFLAGS) $(WANAL_CXXFLAGS)
if MAKE_NO_PRINT_DIRECTORY
AM_MAKEFLAGS = --no-print-directory
endif

ygorincludedir = $(includedir)/$(PACKAGE)
ygorexecdir = $(libexecdir)/$(PACKAGE)-$(VERSION)

EXTRA_DIST =
EXTRA_DIST += LICENSE
EXTRA_DIST += bin/ygor-configure
EXTRA_DIST += bin/ygor-run
EXTRA_DIST += ygor/__init__.py
EXTRA_DIST += ygor/collect.pyx

ygorinclude_HEADERS =
ygorinclude_HEADERS += include/ygor/armnod.h
ygorinclude_HEADERS += include/ygor/data.h
ygorinclude_HEADERS += include/ygor/guacamole.h

noinst_HEADERS =
noinst_HEADERS += common.h
noinst_HEADERS += ygor-internal.h
noinst_HEADERS += visibility.h

lib_LTLIBRARIES = libygor.la
libygor_la_SOURCES =
libygor_la_SOURCES += armnod.cc
libygor_la_SOURCES += data.cc
libygor_la_SOURCES += guacamole.cc
libygor_la_SOURCES += guacamole_amd64.s
libygor_la_LIBADD =
libygor_la_LIBADD += $(E_LIBS)
libygor_la_LIBADD += $(PO6_LIBS)

bin_PROGRAMS = bin/ygor
ygorexec_SCRIPTS =
ygorexec_SCRIPTS += bin/ygor-configure
ygorexec_SCRIPTS += bin/ygor-run
ygorexec_PROGRAMS =
ygorexec_PROGRAMS += ygor-guacamole
ygorexec_PROGRAMS += ygor-armnod
ygorexec_PROGRAMS += ygor-self-measure
ygorexec_PROGRAMS += ygor-inspect
ygorexec_PROGRAMS += ygor-cdf
ygorexec_PROGRAMS += ygor-timeseries
ygorexec_PROGRAMS += ygor-merge

bin_ygor_SOURCES = ygor-cli.cc
bin_ygor_CPPFLAGS = -DYGOR_EXEC_DIR=\""$(ygorexecdir)\"" $(AM_CPPFLAGS) $(CPPFLAGS)
bin_ygor_LDADD = $(E_LIBS) $(PO6_LIBS)

ygor_guacamole_SOURCES = ygor-guacamole.cc
ygor_guacamole_LDADD = libygor.la

ygor_armnod_SOURCES = ygor-armnod.cc
ygor_armnod_LDADD = libygor.la

ygor_self_measure_SOURCES = ygor-self-measure.cc
ygor_self_measure_LDADD = libygor.la

ygor_inspect_SOURCES = ygor-inspect.cc common.cc
ygor_inspect_LDADD = libygor.la

ygor_cdf_SOURCES = ygor-cdf.cc common.cc
ygor_cdf_LDADD = libygor.la

ygor_timeseries_SOURCES = ygor-timeseries.cc common.cc
ygor_timeseries_LDADD = libygor.la

ygor_merge_SOURCES = ygor-merge.cc common.cc
ygor_merge_LDADD = libygor.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ygor.pc

pkgpyexecdir = $(pyexecdir)/ygor
pkgpyexec_DATA =
pkgpyexec_LTLIBRARIES =
if ENABLE_PYTHON_BINDINGS
pkgpyexec_DATA += ygor/__init__.py
pkgpyexec_LTLIBRARIES += ygor/collect.la
endif

ygor_collect_la_SOURCES = ygor/collect.c
ygor_collect_la_CPPFLAGS =
ygor_collect_la_CPPFLAGS += $(PYTHON_CPPFLAGS)
ygor_collect_la_CPPFLAGS += $(AM_CPPFLAGS)
ygor_collect_la_CPPFLAGS += $(CPPFLAGS)
ygor_collect_la_CFLAGS =
ygor_collect_la_CFLAGS += $(PO6_CFLAGS) $(E_CFLAGS)
ygor_collect_la_CFLAGS += -fvisibility=default
ygor_collect_la_CFLAGS += $(CFLAGS)
ygor_collect_la_LIBADD =
ygor_collect_la_LIBADD += libygor.la
ygor_collect_la_LIBADD += $(PYTHON_LDFLAGS)
ygor_collect_la_LDFLAGS = -module -avoid-version -export-symbols-regex $(PYTHON_SYMBOL)collect $(AM_LDFLAGS) $(LDFLAGS)
pyx_verbose = $(pyx_verbose_$(V))
pyx_verbose_ = $(pyx_verbose_$(AM_DEFAULT_VERBOSITY))
pyx_verbose_0 = @echo "  PYX   " $@;
ygor/collect.c: ygor/collect.pyx
	$(pyx_verbose)cython ygor/collect.pyx
