AM_CFLAGS = @GLIB_CFLAGS@ -I/usr/include/wireshark

bin_PROGRAMS = signallogger

signallogger_SOURCES = logger.c
signallogger_LDADD= @GLIB_LIBS@ -lwiretap

