INCLUDES=

bin_PROGRAMS=udriver
udriver_SOURCES=driver.cpp
udriver_LDFLAGS =
udriver_DEPENDENCIES = libuspell.la
udriver_LDADD = libuspell.la -lm

lib_LTLIBRARIES = libuspell.la

libuspell_la_LIBADD= $(ENCHANT_LIBS)
libuspell_la_LDFLAGS = -version-info $(VERSION_INFO) -no-undefined
libuspell_la_SOURCES = 	\
	lookup2.cpp	\
	transcribe.cpp	\
	uniprops.cpp	\
	uspell.cpp	\
	utf8convert.cpp	\
	lookup2.h	\
	myparameters.h	\
	mytypes.h	\
	transcribe.h	\
	uniprops.h	\
	uspell.h	\
	utf8convert.h

libuspell_includedir = $(includedir)/uspell/
libuspell_include_HEADERS = \
	lookup2.h	\
	myparameters.h	\
	mytypes.h	\
	transcribe.h	\
	uniprops.h	\
	uspell.h	\
	utf8convert.h
