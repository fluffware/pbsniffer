AM_CFLAGS = @GLIB_CFLAGS@ -I/usr/include/wireshark -D_XOPEN_SOURCE=600

noinst_PROGRAMS =  pbwriter

pbwriter_SOURCES = pbwriter.c
pbwriter_LDADD= @GLIB_LIBS@ -lwiretap
