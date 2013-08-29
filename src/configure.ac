#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([proposer.cpp])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([qpidrecovery], [0.0])
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lqmfconsole':
AC_CHECK_LIB([qmfconsole], [main])
# FIXME: Replace `main' with a function in `-lqpidclient':
AC_CHECK_LIB([qpidclient], [main])
# FIXME: Replace `main' with a function in `-lqpidcommon':
AC_CHECK_LIB([qpidcommon], [main])
# FIXME: Replace `main' with a function in `-lqpidmessaging':
AC_CHECK_LIB([qpidmessaging], [main])
# FIXME: Replace `main' with a function in `-lqpidtypes':
AC_CHECK_LIB([qpidtypes], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h memory.h netdb.h netinet/in.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 gethostbyaddr gethostbyname gettimeofday inet_ntoa memset select socket strchr strerror strtol strtoul])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
