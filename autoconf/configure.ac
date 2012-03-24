dnl **************************************************************************
dnl * Initialize
dnl **************************************************************************
AC_INIT([[[slicer]]],[[[0.10]]],[jingyue@cs.columbia.edu])

dnl Compile-time options
AC_CONFIG_HEADERS([config.h])

dnl Identify where LLVM source tree is
LLVM_SRC_ROOT="."
LLVM_OBJ_ROOT="."

dnl Find absolute paths to LLVM source and object trees
LLVM_ABS_SRC_ROOT="`cd $srcdir ; cd $LLVM_SRC_ROOT ; pwd`"
LLVM_ABS_OBJ_ROOT="`cd $LLVM_OBJ_ROOT ; pwd`"

dnl Tell autoconf that this is an LLVM project being configured
dnl This provides the --with-llvmsrc and --with-llvmobj options
LLVM_CONFIG_PROJECT($LLVM_ABS_SRC_ROOT,$LLVM_ABS_OBJ_ROOT)

dnl Tell autoconf that the auxilliary files are actually located in
dnl the LLVM autoconf directory, not here.
AC_CONFIG_AUX_DIR($LLVM_SRC/autoconf)

dnl Verify that the source directory is valid
AC_CONFIG_SRCDIR(["Makefile.common.in"])

dnl Configure a common Makefile
AC_CONFIG_FILES(Makefile.common)

dnl Configure project makefiles
dnl List every Makefile that exists within your source tree
AC_CONFIG_MAKEFILE(Makefile)

dnl **************************************************************************
dnl * Determine which system we are building on
dnl **************************************************************************

dnl **************************************************************************
dnl * Check for programs.
dnl **************************************************************************

dnl **************************************************************************
dnl * Check for libraries.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for header files.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for typedefs, structures, and compiler characteristics.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for library functions.
dnl **************************************************************************

dnl **************************************************************************
dnl * Enable various compile-time options
dnl **************************************************************************
AC_ARG_ENABLE(bound-check, AS_HELP_STRING([--enable-bound-check], [Enable bound checks (default = NO)]), , enableval=no)
if test "$enableval" = yes; then
	AC_SUBST(CHECK_BOUND, [1])
else
	AC_SUBST(CHECK_BOUND, [0])
fi
AC_DEFINE_UNQUOTED([CHECK_BOUND], $CHECK_BOUND, [Enable bound checks])

AC_ARG_ENABLE(div-check, AS_HELP_STRING([--enable-div-check], [Enable div-by-zero checks (default = YES)]), , enableval=yes)
if test "$enableval" = yes; then
	AC_SUBST(CHECK_DIV, [1])
else
	AC_SUBST(CHECK_DIV, [0])
fi
AC_DEFINE_UNQUOTED([CHECK_DIV], $CHECK_DIV, [Enable div-by-zero checks])

dnl **************************************************************************
dnl * Set the location of various third-party software packages
dnl **************************************************************************

dnl **************************************************************************
dnl * Create the output files
dnl **************************************************************************

dnl This must be last
AC_OUTPUT
