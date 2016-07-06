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

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory
AM_CPPFLAGS = $(PO6_CFLAGS) $(E_CFLAGS)
AM_CFLAGS   = -fvisibility=hidden $(PO6_CFLAGS) $(E_CFLAGS) $(WANAL_CFLAGS)
AM_CXXFLAGS = -fvisibility=hidden -fvisibility-inlines-hidden $(PO6_CFLAGS) $(E_CFLAGS) $(WANAL_CXXFLAGS)

CLEANFILES =

EXTRA_DIST  =
EXTRA_DIST += LICENSE
EXTRA_DIST += bin/ygor-configure
EXTRA_DIST += bin/ygor-run
EXTRA_DIST += bin/ygor-graph
EXTRA_DIST += ygor/__init__.py
EXTRA_DIST += ygor/collect.pyx
EXTRA_DIST += ygor/graph.py

bin_PROGRAMS = bin/ygor
bin_ygor_SOURCES = ygor-cli.cc
bin_ygor_CPPFLAGS = -DYGOR_EXEC_DIR=\""$(ygorexecdir)\"" $(AM_CPPFLAGS) $(CPPFLAGS)
bin_ygor_LDADD = $(E_LIBS) $(PO6_LIBS)

ygorexecdir = $(libexecdir)/$(PACKAGE)-$(VERSION)
ygorexec_SCRIPTS  =
ygorexec_SCRIPTS += bin/ygor-configure
ygorexec_SCRIPTS += bin/ygor-run
ygorexec_SCRIPTS += bin/ygor-graph
ygorexec_PROGRAMS  =
ygorexec_PROGRAMS += bin/ygor-armnod
ygorexec_PROGRAMS += bin/ygor-cdf
ygorexec_PROGRAMS += bin/ygor-percentiles
ygorexec_PROGRAMS += bin/ygor-dump
ygorexec_PROGRAMS += bin/ygor-guacamole
ygorexec_PROGRAMS += bin/ygor-merge
ygorexec_PROGRAMS += bin/ygor-summarize
ygorexec_PROGRAMS += bin/ygor-timeseries
ygorexec_PROGRAMS += bin/ygor-t-test

bin_ygor_armnod_SOURCES = ygor-armnod.cc
bin_ygor_armnod_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_cdf_SOURCES = ygor-cdf.cc
bin_ygor_cdf_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_percentiles_SOURCES = ygor-percentiles.cc
bin_ygor_percentiles_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_dump_SOURCES = ygor-dump.cc
bin_ygor_dump_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_guacamole_SOURCES = ygor-guacamole.cc
bin_ygor_guacamole_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_merge_SOURCES = ygor-merge.cc
bin_ygor_merge_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_summarize_SOURCES = ygor-summarize.cc
bin_ygor_summarize_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_timeseries_SOURCES = ygor-timeseries.cc
bin_ygor_timeseries_LDADD = libygor.la $(PO6_LIBS)

bin_ygor_t_test_SOURCES = ygor-t-test.cc
bin_ygor_t_test_LDADD = libygor.la $(PO6_LIBS)

include_HEADERS = ygor.h
noinst_HEADERS = ygor-internal.h guacamole.h visibility.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ygor.pc

lib_LTLIBRARIES = libygor.la

libygor_la_SOURCES = ygor.cc armnod.cc guacamole.cc
libygor_la_LIBADD = $(E_LIBS) $(PO6_LIBS)

pkgpyexecdir = $(pyexecdir)/ygor
pkgpyexec_DATA =
pkgpyexec_LTLIBRARIES =
if ENABLE_PYTHON_BINDINGS
pkgpyexec_DATA += ygor/__init__.py
pkgpyexec_DATA += ygor/graph.py
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

TESTS  =
TESTS += test-salsa208-guacamole

check_PROGRAMS = test-salsa208-guacamole

test_salsa208_guacamole_SOURCES = test-salsa208-guacamole.cc guacamole.cc
test_salsa208_guacamole_CPPFLAGS =
test_salsa208_guacamole_CPPFLAGS += $(AM_CPPFLAGS)
test_salsa208_guacamole_CPPFLAGS += $(CPPFLAGS)
test_salsa208_guacamole_LDADD = $(E_LIBS) -lsodium

##################################### Java #####################################

ygor_jarfile = bindings/java/net.rescrv.ygor-$(VERSION).jar

jardir = $(datadir)/java
jar_DATA =
if ENABLE_JAVA_BINDINGS
lib_LTLIBRARIES += libygor-java.la
jar_DATA += $(ygor_jarfile)
endif

CLEANFILES += $(ygor_jarfile)
CLEANFILES += bindings/java/net/rescrv/ygor/*.class

EXTRA_DIST += bindings/java/net/rescrv/ygor/DataLogger.java

noinst_HEADERS += bindings/java/net_rescrv_ygor_DataLogger.h

libygor_java_la_SOURCES = bindings/java/net_rescrv_ygor_DataLogger.c
libygor_java_la_CPPFLAGS = $(JNI_CPPFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS)
libygor_java_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS) -Wmissing-prototypes
libygor_java_la_LIBADD = libygor.la

JAVA_SRC_BASE=${abs_top_srcdir}/bindings/java
JAVA_BUILD_BASE=${abs_top_builddir}/bindings/java
JAVA_SRC_PKG=${JAVA_SRC_BASE}/net/rescrv/ygor
JAVA_BUILD_PKG=${JAVA_BUILD_BASE}/net/rescrv/ygor

$(ygor_jarfile): $(wildcard ${JAVA_SRC_PKG}/*.java)
	javac -d ${JAVA_BUILD_BASE} ${JAVA_SRC_PKG}/*.java
	cd ${JAVA_BUILD_BASE} && $(JAR) cvf $(JARFLAGS) ${abs_top_builddir}/${ygor_jarfile} net/rescrv/ygor/*.class
