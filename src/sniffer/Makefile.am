AM_CFLAGS = @GLIB_CFLAGS@

bin_PROGRAMS = pbsniffer pcapngtst

pbsniffer_SOURCES = sniffer.c \
serial.c serial.h \
pcapng.c pcapng.h

pbsniffer_LDADD= @GLIB_LIBS@ 

pcapngtst_SOURCES = pcapngtst.c \
pcapng.c pcapng.h

pcapngtst_LDADD= @GLIB_LIBS@ 
