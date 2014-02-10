AC_INIT([pbsniffer], [0.1])
AC_CONFIG_SRCDIR([src/sniffer/sniffer.c])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CC_C99
AC_SYS_LARGEFILE
AM_PATH_GLIB_2_0(2.30.0,,, [gobject gio gio-unix])

AC_CONFIG_FILES([Makefile src/Makefile src/sniffer/Makefile src/writer/Makefile])
AC_OUTPUT
