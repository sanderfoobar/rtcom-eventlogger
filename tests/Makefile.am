## Process this file with automake to produce Makefile.in

INCLUDES = $(RTCOM_EVENTLOGGER_CFLAGS) \
           -I$(srcdir)/..

bin_PROGRAMS = rtcom-eventlogger-testsuite

rtcom_eventlogger_testsuite_SOURCES = \
	canned-data.c \
	check_el.c \
	check_db.c \
	fail.h
rtcom_eventlogger_testsuite_CFLAGS = $(CHECK_FLAGS)
rtcom_eventlogger_testsuite_LDADD = \
	$(RTCOM_EVENTLOGGER_LIBS) \
	$(CHECK_LIBS) ${top_builddir}/src/librtcom-eventlogger.la

testdir = $(libdir)/rtcom-eventlogger/plugins
test_LTLIBRARIES = test.la
test_includedir = $(includedir)
test_la_SOURCES = test-plugin.c
test_la_LIBADD = $(RTCOM_EVENTLOGGER_LIBS)
test_la_LDFLAGS = -module -avoid-version

COMMON_TESTS_ENVIRONMENT = \
	RTCOM_EL_PLUGINS_PATH=.libs \
	RTCOM_EL_HOME=@abs_top_builddir@/tests \
	G_DEBUG=fatal-criticals
TESTS_ENVIRONMENT = \
	$(COMMON_TESTS_ENVIRONMENT) \
	sh @top_srcdir@/tools/with-session-bus.sh --session --

TESTS = rtcom-eventlogger-testsuite

check-valgrind:
	$(MAKE) check-TESTS TESTS_ENVIRONMENT='$$(VALGRIND_TESTS_ENVIRONMENT)'

VALGRIND_TESTS_ENVIRONMENT = \
	$(COMMON_TESTS_ENVIRONMENT) \
	G_SLICE=always-malloc \
	G_DEBUG=gc-friendly,fatal-criticals \
	CK_FORK=no \
	sh @top_srcdir@/tools/with-session-bus.sh --session -- \
	$(top_builddir)/libtool --mode=execute \
	valgrind --tool=memcheck --leak-check=full --show-reachable=yes -v \
		--suppressions=$(top_srcdir)/data/rtcom-eventlogger.supp \
		--num-callers=20 --error-exitcode=42 --gen-suppressions=all \
		$(VALGRIND_FLAGS)
