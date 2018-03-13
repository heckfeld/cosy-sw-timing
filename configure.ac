#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([sw_timing], [1.0], [mi.simon@fz-juelich.de])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([sw_timing.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyaddr gethostbyname gethostname inet_ntoa memset select socket])

AC_CONFIG_FILES([Makefile init.d/sw_timing default/sw_timing])
AC_OUTPUT
