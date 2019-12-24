
if GENERATE_DOCS

ref_DATA = html/*
refdir = ${docdir}/reference

EXTRA_DIST = $(ref_DATA)

.PHONY: html

html/*:
	(cd .. && ${DOXYGEN})

endif

CLEANFILES = html/*

