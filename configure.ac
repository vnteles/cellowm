#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([cellowm], [1.0.1], [none@none.com])
AM_INIT_AUTOMAKE(cellowm, 1.0.1)
AC_OUTPUT(Makefile src/Makefile)
AC_CONFIG_SRCDIR([src/main.c])


# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit memchr strtol])

AC_OUTPUT
