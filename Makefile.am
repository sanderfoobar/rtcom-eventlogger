## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I tools

SUBDIRS = tools src tests client data doc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rtcom-eventlogger.pc

libeventloggerdocdir = ${docdir}
libeventloggerdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS

nobase_include_HEADERS = rtcom-eventlogger/eventlogger.h \
                         rtcom-eventlogger/eventlogger-iter.h \
                         rtcom-eventlogger/eventlogger-attach-iter.h \
                         rtcom-eventlogger/eventlogger-plugin.h \
                         rtcom-eventlogger/eventlogger-types.h \
                         rtcom-eventlogger/event.h \
                         rtcom-eventlogger/eventlogger-query.h \
                         rtcom-eventlogger/db.h

EXTRA_DIST = $(libeventloggerdoc_DATA)

LCOV_DIRS = src
include tools/lcov.am
