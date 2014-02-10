AM_CFLAGS = @GLIB_CFLAGS@ -I/usr/include/wireshark

noinst_PROGRAMS =  pbwriter

pbwriter_SOURCES = pbwriter.c
pbwriter_LDADD= @GLIB_LIBS@ -lwiretap
