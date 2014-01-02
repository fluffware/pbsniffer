AM_CFLAGS = @GLIB_CFLAGS@

bin_PROGRAMS = pbsniffer hex2pcapng pcapngtst

pbsniffer_SOURCES = sniffer.c \
serial.c serial.h \
pcapng.c pcapng.h \
write_libpcap.c write_libpcap.h

pbsniffer_LDADD= @GLIB_LIBS@ 

pcapngtst_SOURCES = pcapngtst.c \
pcapng.c pcapng.h

pcapngtst_LDADD= @GLIB_LIBS@ 

hex2pcapng_SOURCES = hex2pcapng.c \
pcapng.c pcapng.h

hex2pcapng_LDADD= @GLIB_LIBS@ 
