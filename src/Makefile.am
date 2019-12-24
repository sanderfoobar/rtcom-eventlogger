## Process this file with automake to produce Makefile.in

INCLUDES = \
	$(RTCOM_EVENTLOGGER_CFLAGS) \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_PLUGINS_DIR=\""$(libdir)/rtcom-eventlogger/plugins"\"

AM_CFLAGS = -I$(srcdir)/..

lib_LTLIBRARIES = librtcom-eventlogger.la

librtcom_eventlogger_includedir = $(includedir)/rtcom-eventlogger
librtcom_eventlogger_la_SOURCES = eventlogger.c \
                                  eventlogger-iter.c \
                                  eventlogger-attach-iter.c \
                                  eventlogger-plugin.c \
                                  event.c \
                                  eventlogger-query.c \
                                  eventlogger-marshalers.c \
                                  db.c

librtcom_eventlogger_la_LIBADD = $(RTCOM_EVENTLOGGER_LIBS)

librtcom_eventlogger_la_LDFLAGS = -no-undefined \
        -export-symbols-regex "^rtcom_el_.*" \
        -version-info "$(LT_CURRENT)":"$(LT_REVISION)":"$(LT_AGE)"

