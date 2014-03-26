AM_CFLAGS = @GLIB_CFLAGS@ -I/usr/include/wireshark @MYSQL_CLIENT_CFLAGS@

bin_PROGRAMS = signallogger

signallogger_SOURCES = logger.c \
	signal_extractor.c signal_extractor.h

signallogger_LDADD= @GLIB_LIBS@ -lwiretap @MYSQL_CLIENT_LIBS@
