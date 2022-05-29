# gp-show-module-vars.m4 - show pkg module variables          -*- Autoconf -*-
# serial 1
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl GP_SHOW_MODULE_VARS(MODULE)
dnl    Show the pkgconfig module variables for the given MODULE.
dnl ####################################################################
dnl
dnl Usage:
dnl     GP_SHOW_MODULE_VARS([LIBEXIF])
dnl Result:
dnl     checking for value of LIBEXIF_CFLAGS... -I/usr/local/include
dnl     checking for value of LIBEXIF_LIBS... -L/usr/local/lib -lexif
dnl
dnl ####################################################################
dnl
AC_DEFUN([GP_SHOW_MODULE_VARS], [dnl
AC_MSG_CHECKING([value of $1_CFLAGS])
AC_MSG_RESULT([${$1_CFLAGS}])
AC_MSG_CHECKING([value of $1_LIBS])
AC_MSG_RESULT([${$1_LIBS}])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
