AM_CFLAGS = @GLIB_CFLAGS@

noinst_PROGRAMS =  pcapngtst hex2pcapng scheduled_time_test
bin_PROGRAMS = pbsniffer pblog

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

pblog_SOURCES=pblog.c \
pbframer.c pbframer.h \
serial.c serial.h \
scheduled_time.c scheduled_time.h\
pcapng.c pcapng.h

pblog_LDADD= @GLIB_LIBS@ 

scheduled_time_test_SOURCES = scheduled_time_test.c \
scheduled_time.c scheduled_time.h

scheduled_time_test_LDADD= @GLIB_LIBS@
