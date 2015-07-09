#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ([2.69])
AC_INIT([ComplexSim], [0.1], [fabrizio.messina@gmail.com])
LT_INIT([shared])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR()
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# FIXME: Replace `main' with a function in `-lgsl':
AC_CHECK_LIB([m],[cos])
AC_CHECK_LIB([gslcblas],[cblas_dgemm])
AC_CHECK_LIB([gsl],[gsl_blas_dgemm])
# FIXME: Replace `main' with a function in `-ligraph':
AC_CHECK_LIB([igraph], [igraph_adjlist_init], [], [echo "Library Igraph not found!"; exit -1])
# FIXME: Replace `main' with a function in `-llog4c':
AC_CHECK_LIB([log4c], [log4c_init], [], [echo "Library log4c not found!"; exit -1])
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([log4c.h igraph/igraph.h gsl/gsl_cblas.h gsl/gsl.h fcntl.h limits.h stddef.h stdlib.h string.h strings.h unistd.h values.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([floor memset sqrt strcasecmp strerror strrchr strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
