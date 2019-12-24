## Process this file with automake to produce Makefile.in

INCLUDES = $(RTCOM_EVENTLOGGER_CFLAGS) \
           -I$(srcdir)/..

bin_PROGRAMS = rtcom-eventlogger-client

rtcom_eventlogger_client_SOURCES = el-client.c
rtcom_eventlogger_client_LDADD = \
	$(RTCOM_EVENTLOGGER_LIBS) \
	${top_builddir}/src/librtcom-eventlogger.la

