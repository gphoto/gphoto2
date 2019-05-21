AC_PREREQ([2.69])
dnl Next stable release must be 2.5.21 or later
AC_INIT([gphoto2 command line interface], [2.5.20.1], [gphoto-devel@lists.sourceforge.net], [gphoto2])
AC_CONFIG_SRCDIR([gphoto2/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([auto-m4])
AC_CONFIG_AUX_DIR([auto-aux])
AM_INIT_AUTOMAKE([-Wall -Werror gnu 1.15 dist-xz check-news silent-rules])

GP_CHECK_SHELL_ENVIRONMENT
GP_CONFIG_MSG([Build])
GP_CONFIG_MSG([Source code location],[${srcdir}])

AX_IS_RELEASE([git-directory])
AX_CHECK_ENABLE_DEBUG()


dnl ---------------------------------------------------------------------------
dnl Some programs we need
dnl ---------------------------------------------------------------------------
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
LT_INIT
AC_PROG_INSTALL
AC_SYS_LARGEFILE

GP_CONFIG_MSG([Compiler],[${CC}])
AC_DEFINE_UNQUOTED([HAVE_CC],"$CC",[The C compiler we're using])

dnl AC_STRUCT_TIMEZONE


GP_CONFIG_MSG([Features])


# ----------------------------------------------------------------------
# References to external resources (web sites, mailing listes, etc.)
# ----------------------------------------------------------------------

GP_REFERENCES()dnl


dnl ---------------------------------------------------------------------------
dnl i18n support
dnl ---------------------------------------------------------------------------
GP_GETTEXT_HACK([],[Lutz Müller and others],[${MAIL_GPHOTO_TRANSLATION}])
AM_GNU_GETTEXT_VERSION([0.19.1])
AM_GNU_GETTEXT([external])
AM_ICONV()
GP_GETTEXT_FLAGS()


dnl ---------------------------------------------------------------------------
dnl Check for libgphoto2
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBGPHOTO2], [libgphoto2], [>= 2.5.17])


AC_CHECK_FUNCS([strptime])

AC_CHECK_FUNCS([signal pause sleep])


AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h process.h signal.h sys/time.h sys/wait.h])

AC_CHECK_FUNCS([spawnve])

AC_CHECK_LIB(m,floor)


dnl ---------------------------------------------------------------------------
dnl pthread: The gphoto2 shell (--shell) keeps up a connection to the 
dnl          camera. Some cameras need to be sent keep-alive-messages
dnl          in order to prevent a shut-down. This is what we need
dnl          pthread for.
dnl ---------------------------------------------------------------------------
pthread_msg="no"
AC_ARG_WITH([pthread],
    AS_HELP_STRING([--without-pthread], [Don't use pthread]),
    [],
    [with_pthread=yes])

AS_IF([test "$with_pthread" = "yes"], [
    AX_PTHREAD([
        AC_DEFINE(HAVE_PTHREAD,1,[Define if we use pthread.h.])
        pthread_msg="yes"
    ])
])
GP_CONFIG_MSG([pthread support],[$pthread_msg])


dnl ---------------------------------------------------------------------------
dnl CDK: If you would like to interactively access the camera's configuration
dnl      using gphoto2, the command-line frontend, you need CDK. For pure
dnl      command-line access to the config, you don't need CDK, though.
dnl      Old versions of CDK don't include fscale, therefore check for it here.
dnl ---------------------------------------------------------------------------
cdk_msg="no (http://dickey.his.com/cdk/cdk.html)"
have_cdk=false
AC_ARG_WITH([cdk],
    AS_HELP_STRING([--without-cdk],[Do not use cdk]),
    [],
    [with_cdk=auto])
AC_ARG_WITH([cdk-prefix],
    AS_HELP_STRING([--with-cdk-prefix=PREFIX], [Location of cdk]),
    [],
    [with_cdk_prefix=$ac_default_prefix])

AS_IF([test "$with_cdk" != "no"], [
        CPPFLAGS_save="$CPPFLAGS"
        CPPFLAGS="-I$with_cdk_prefix/include $CPPFLAGS"
        AC_CHECK_HEADER([cdk/cdk.h], [
                have_cdk=:
                AC_CHECK_TOOL([CDK_CONFIG], [cdk5-config])
                AS_IF([test -n "${CDK_CONFIG}"], [
                        CDK_CFLAGS=`$CDK_CONFIG --cflags`
                        CDK_LIBS=`$CDK_CONFIG --libs`
                ], [
                        CDK_CFLAGS="-I$with_cdk_prefix/include"
                        CDK_LIBS="-L$with_cdk_prefix/lib -lcdk -lncurses"
                ])
                AC_DEFINE([HAVE_CDK],1,[whether we have CDK])
                LDFLAGS_save="$LDFLAGS"
                LDFLAGS="$CDK_LIBS"
                AC_CHECK_LIB([cdk], [newCDKFScale], [cdk_msg="yes"], [
                        AC_DEFINE([HAVE_CDK_20010421],1,
                                  [whether we have old CDK])
                        cdk_msg="yes (but without fscale)"
                ])
                LDFLAGS="$LDFLAGS_save"
        ])
        CPPFLAGS="$CPPFLAGS_save"
], [
        cdk_msg="no (not requested)"
])
AM_CONDITIONAL([HAVE_CDK], [$have_cdk])
AC_SUBST([CDK_CFLAGS])
AC_SUBST([CDK_LIBS])
GP_CONFIG_MSG([Interactive config support],[$cdk_msg])

dnl We emulate signals in Windows
dnl This is taken from: https://github.com/OpenVPN/openvpn/blob/master/configure.ac
AC_CHECK_DECLS(
	[SIGHUP],
	,
	[AC_DEFINE([SIGHUP], [1], [SIGHUP replacement])],
	[[
		#ifdef HAVE_SIGNAL_H
		#include <signal.h>
		#endif
	]]
)
AC_CHECK_DECLS(
	[SIGINT],
	,
	[AC_DEFINE([SIGINT], [2], [SIGINT replacement])],
	[[
		#ifdef HAVE_SIGNAL_H
		#include <signal.h>
		#endif
	]]
)
AC_CHECK_DECLS(
	[SIGUSR1],
	,
	[AC_DEFINE([SIGUSR1], [10], [SIGUSR1 replacement])],
	[[
		#ifdef HAVE_SIGNAL_H
		#include <signal.h>
		#endif
	]]
)
AC_CHECK_DECLS(
	[SIGUSR2],
	,
	[AC_DEFINE([SIGUSR2], [12], [SIGUSR2 replacement])],
	[[
		#ifdef HAVE_SIGNAL_H
		#include <signal.h>
		#endif
	]]
)
AC_CHECK_DECLS(
	[SIGTERM],
	,
	[AC_DEFINE([SIGTERM], [15], [SIGTERM replacement])],
	[[
		#ifdef HAVE_SIGNAL_H
		#include <signal.h>
		#endif
	]]
)

dnl ---------------------------------------------------------------------------
dnl libjpeg: Right now, libjpeg is only used to interpret JPEG previews in
dnl          order to feed them to aalib (see below).
dnl ---------------------------------------------------------------------------
jpeg_msg="no"
have_jpeg=false
AC_ARG_WITH([jpeg],
    AS_HELP_STRING([--without-jpeg],[Do not use jpeg]),
    [],
    [with_jpeg=auto])
AC_ARG_WITH([jpeg-prefix],
    AS_HELP_STRING([--with-jpeg-prefix=PREFIX], [Location of jpeg]),
    [],
    [with_jpeg_prefix=$ac_default_prefix])

AS_IF([test "$with_jpeg" != "no"], [
        CPPFLAGS_save="$CPPFLAGS"
        CPPFLAGS="-I$with_jpeg_prefix/include"
        AC_CHECK_HEADER([jpeglib.h], [
                AC_CHECK_LIB([jpeg], [jpeg_read_scanlines], [
                        have_jpeg=:
                        jpeg_msg="yes"
                        AC_DEFINE([HAVE_JPEG],1,[whether we use libjpeg])
                        JPEG_LIBS="-ljpeg"
                        JPEG_CFLAGS=$CPPFLAGS
                ], [
                        LDFLAGS_save="$LDFLAGS"
                        LDFLAGS="-L$with_jpeg_prefix/lib"
                        AC_CHECK_LIB([jpeg], [jpeg_stdio_src], [
                                have_jpeg=:
                                jpeg_msg="yes (in '$with_jpeg_prefix')"
                                AC_DEFINE([HAVE_JPEG],1,
                                          [whether we use libjpeg])
                                JPEG_LIBS="-L$with_jpeg_prefix/lib -ljpeg"
                                JPEG_CFLAGS=$CPPFLAGS
                        ], [
                                jpeg_msg="no (could not link)"
                        ])
                        LDFLAGS=$LDFLAGS_save
                ])
        ])
        CPPFLAGS="$CPPFLAGS_save"
], [
        jpeg_msg="no (not requested)"
])
AM_CONDITIONAL([HAVE_JPEG], [$have_jpeg])
AC_SUBST([JPEG_LIBS])
AC_SUBST([JPEG_CFLAGS])
GP_CONFIG_MSG([JPEG support],[$jpeg_msg])


dnl ---------------------------------------------------------------------------
dnl libexif: The CameraFilesystem can use libexif for extracting thumbnails
dnl          out of EXIF data. Similarly, it can extract the mtime of
dnl          a file. 
dnl          libexif is available from
dnl          http://www.sourceforge.net/projects/libexif
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBEXIF],[libexif],[>= 0.3.2],
                 [libexif/exif-data.h],[exif_data_new],[
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="${LIBEXIF_CFLAGS}"
	AC_CHECK_HEADER([libexif/exif-log.h], [
		AC_DEFINE([HAVE_LIBEXIF_LOG],1,
                          [whether libexif has exif-log.h (>= 0.6.9)])dnl
        ])
	AC_CHECK_HEADER([libexif/exif-ifd.h], [
		AC_DEFINE([HAVE_LIBEXIF_IFD],1,
                          [whether libexif has exif-ifd.h (>= 0.5.3)])dnl
        ])
	CPPFLAGS="$CPPFLAGS_save"
],[],[default-on])


dnl ---------------------------------------------------------------------------
dnl aalib: gphoto2 (the frontend) has been designed as a command-line tool.
dnl        However, live previews make only sense when you can actually
dnl        see the preview. This is where aalib fits in - it gives you the
dnl        possibility to view live previews on any console.
dnl ---------------------------------------------------------------------------
aa_msg="no (http://www.sourceforge.net/projects/aa-project)"
have_aa=false
AC_ARG_WITH([aalib],
    AS_HELP_STRING([--with-aalib],[Do use aalib for preview ASCII rendering]),
    [],
    [with_aalib=auto])

AS_IF([test "$with_aalib" != "no"], [
        AC_CHECK_HEADER([aalib.h],[
                have_aa=:
                aa_msg=yes
                AC_DEFINE([HAVE_AA], [1], [whether we use aalib])
                AC_CHECK_TOOL([AALIB_CONFIG], [aalib-config])
                AS_IF([test -n "${AALIB_CONFIG}"], [
                        AA_CFLAGS=`$AALIB_CONFIG --cflags`
                        AA_LIBS=`$AALIB_CONFIG --libs`
                ], [
                        AA_LIBS="-laa"
                ])
        ])
], [
        aa_msg="no (not requested)"
])
AM_CONDITIONAL([HAVE_AA], [$have_aa])
AC_SUBST([AA_LIBS])
AC_SUBST([AA_CFLAGS])
GP_CONFIG_MSG([Text preview support],[$aa_msg])


dnl ---------------------------------------------------------------------------
dnl popt.h: Simplifies handling of command-line options enormously.
dnl ---------------------------------------------------------------------------
GP_CHECK_POPT([mandatory])


dnl ---------------------------------------------------------------------------
dnl readline: Readline is pretty cool - it allows you to use <tab> to complete
dnl           commands in the gphoto2 shell or to use <up> or <down> to 
dnl           navigate through the history.
dnl ---------------------------------------------------------------------------
rl_msg="no (http://cnswww.cns.cwru.edu/~chet/readline/rltop.html)"
have_readline=false
AC_ARG_WITH([readline],
    AS_HELP_STRING([--without-readline], [Do not use readline]),
    [],
    [with_readline=auto])

AS_IF([test "$with_readline" != "no"], [
        AC_CHECK_HEADER([readline/readline.h], [
            AC_CHECK_LIB([readline], [rl_completion_append_character], [
                AC_CHECK_LIB([readline], [rl_completion_matches], [
                    have_readline=:
                    rl_msg="yes (doesn't require explicit -lncurses)"
                    RL_LIBS="-lreadline"
                    RL_CFLAGS=""
                    AC_DEFINE([HAVE_RL], [1], [whether we use readline])
                ], [
                    rl_msg="no (available version too old)"
                ])
            ], [
                LDFLAGS_save="$LDFLAGS"
                LDFLAGS="-lncurses"
                AC_CHECK_LIB([readline], [rl_completion_matches], [
                    have_readline=:
                    rl_msg="yes (requires explicit -lncurses)"
                    RL_LIBS="-lreadline -lncurses"
                    RL_CFLAGS=""
                    AC_DEFINE(HAVE_RL,1,[whether we use readline])
                ], [
                    rl_msg="no (couldn't link against it or version too old)"
                ])
                LDFLAGS="$LDFLAGS_save"
            ])
        ])
], [
        rl_msg="no (not requested)"
])
AM_CONDITIONAL([HAVE_RL], [$have_readline])
AC_SUBST([RL_LIBS])
AC_SUBST([RL_CFLAGS])
GP_CONFIG_MSG([Readline support], [$rl_msg])


dnl ---------------------------------------------------------------------------
dnl Turn on all warnings when using gcc
dnl ---------------------------------------------------------------------------
AX_APPEND_COMPILE_FLAGS([-Wall -Wmissing-declarations -Wmissing-prototypes])


# ---------------------------------------------------------------------------
# Create output files
# ---------------------------------------------------------------------------
AC_CONFIG_FILES([
po/Makefile.in 
Makefile
gphoto2/Makefile
gphoto-m4/Makefile
doc/Makefile
contrib/Makefile
packaging/Makefile
packaging/rpm/Makefile
packaging/rpm/package.spec
tests/data/Makefile
tests/staging/subdir1/Makefile
tests/staging/subdir2/Makefile
tests/staging/Makefile
tests/Makefile
])
AC_OUTPUT()dnl

# Status output
GP_CONFIG_OUTPUT
