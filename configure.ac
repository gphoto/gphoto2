AC_PREREQ(2.59)
dnl Next stable release must be 2.4.11 or later
AC_INIT([gphoto2 command line interface], [2.4.11], [gphoto-devel@lists.sourceforge.net], [gphoto2])
AC_CONFIG_SRCDIR([gphoto2/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([auto-m4])
AC_CONFIG_AUX_DIR([auto-aux])
AM_INIT_AUTOMAKE([-Wall -Werror gnu 1.9 dist-bzip2 check-news])

if test ! -d "$srcdir/m4m"; then
AC_MSG_ERROR([
You are missing the m4m/ directory in your top
$PACKAGE_TARNAME source directory.

You are probably using an ill-maintained SVN tree.
Running

    cd $srcdir
    svn up

and re-running autogen.sh might help.
])
fi

GP_CHECK_SHELL_ENVIRONMENT
GP_CONFIG_MSG([Build])
GP_CONFIG_MSG([Source code location],[${srcdir}])
AM_CPPFLAGS="$CPPFLAGS"
AM_LDFLAGS="$LDFLAGS"


dnl ---------------------------------------------------------------------------
dnl Some programs we need
dnl ---------------------------------------------------------------------------
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
AM_PROG_LIBTOOL
AC_PROG_INSTALL
AC_SYS_LARGEFILE
GP_PKG_CONFIG

GP_CONFIG_MSG([Compiler],[${CC}])
AC_DEFINE_UNQUOTED([HAVE_CC],"$CC",[The C compiler we're using])

AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)"
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)/gphoto2"

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
ALL_LINGUAS="az cs da de en_GB es eu fi fr hu id it ja nl pa pl pt_BR ro ru rw sk sr sv uk vi zh_CN zh_TW"
AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
AM_PO_SUBDIRS()
AM_ICONV()
GP_GETTEXT_FLAGS()

dnl We cannot use AC_DEFINE_UNQUOTED() for these definitions, as
dnl we require make to do insert the proper $(datadir) value
AC_SUBST([localedir],["\$(datadir)/locale"])
AM_CPPFLAGS="$AM_CPPFLAGS -DLOCALEDIR=\\\"${localedir}\\\""


dnl ---------------------------------------------------------------------------
dnl Check for libgphoto2
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBGPHOTO2], [libgphoto2], [>= 2.4.11])


AC_CHECK_FUNCS([strptime])

AC_CHECK_FUNCS([signal pause sleep])


AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h])


dnl ---------------------------------------------------------------------------
dnl pthread: The gphoto2 shell (--shell) keeps up a connection to the 
dnl          camera. Some cameras need to be sent keep-alive-messages
dnl          in order to prevent a shut-down. This is what we need
dnl          pthread for.
dnl ---------------------------------------------------------------------------
pthread_msg=no
try_pthread=:
AC_ARG_WITH(pthread, [  --without-pthread         Don't use pthread],
        if test x$withval = xno; then
                try_pthread=false
        fi)
if $try_pthread; then
        AC_CHECK_HEADER(pthread.h, [
                AC_CHECK_LIB(pthread, pthread_exit, [
                    AC_DEFINE(HAVE_PTHREAD,1,[Define if we use pthread.h.])
                    pthread_msg="yes (-lpthread)"
                    PTHREAD_LIBS="-lpthread"],[
                    AC_CHECK_LIB(c_r, pthread_create, [ 
                        AC_DEFINE(HAVE_PTHREAD,1,[Define if we use pthread.h.])
                        pthread_msg="yes (-pthread)"
                        PTHREAD_LIBS="-pthread"
                    ])
                ])
        ])
fi
AC_SUBST([PTHREAD_LIBS])
GP_CONFIG_MSG([pthread support],[$pthread_msg])


dnl ---------------------------------------------------------------------------
dnl CDK: If you would like to interactively access the camera's configuration
dnl      using gphoto2, the command-line frontend, you need CDK. For pure
dnl      command-line access to the config, you don't need CDK, though.
dnl      Old versions of CDK don't include fscale, therefore check for it here.
dnl ---------------------------------------------------------------------------
cdk_msg="no (http://dickey.his.com/cdk/cdk.html)"
try_cdk=:
have_cdk=false
AC_ARG_WITH([cdk],AS_HELP_STRING([--without-cdk],[Do not use cdk]), [
        if test x$withval = xno; then
                try_cdk=false
                cdk_msg="no (not requested)"
        fi
])
cdk_prefix=$ac_default_prefix
AC_ARG_WITH([cdk-prefix],AS_HELP_STRING([--with-cdk-prefix=PREFIX],
                                        [Location of cdk]), [
        cdk_prefix="$withval"
])
if $try_cdk; then
        CPPFLAGS_save="$CPPFLAGS"
        CPPFLAGS="-I$cdk_prefix/include $CPPFLAGS"
        AC_CHECK_HEADER([cdk/cdk.h], [
                have_cdk=:
                CDK_CFLAGS="-I$cdk_prefix/include"
                CDK_LIBS="-L$cdk_prefix/lib -lcdk -lncurses"
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
fi
AM_CONDITIONAL([HAVE_CDK], [$have_cdk])
AC_SUBST([CDK_CFLAGS])
AC_SUBST([CDK_LIBS])
GP_CONFIG_MSG([Interactive config support],[$cdk_msg])


dnl ---------------------------------------------------------------------------
dnl libjpeg: Right now, libjpeg is only used to interpret JPEG previews in
dnl          order to feed them to aalib (see below).
dnl ---------------------------------------------------------------------------
jpeg_msg=no
try_jpeg=:
have_jpeg=false
AC_ARG_WITH([jpeg],AS_HELP_STRING([--without-jpeg],[Do not use jpeg]),[
        if test x$withval = xno; then
                try_jpeg=false
                jpeg_msg="no (not requested)"
        fi
])
jpeg_prefix=$ac_default_prefix
AC_ARG_WITH(jpeg-prefix, AS_HELP_STRING([--with-jpeg-prefix=PREFIX],
                                        [Location of jpeg]), [
        jpeg_prefix="$withval"
])
if $try_jpeg; then
        CPPFLAGS_save="$CPPFLAGS"
        CPPFLAGS="-I$jpeg_prefix/include"
        AC_CHECK_HEADER([jpeglib.h], [
                AC_CHECK_LIB([jpeg], [jpeg_read_scanlines], [
                        have_jpeg=:
                        jpeg_msg="yes"
                        AC_DEFINE([HAVE_JPEG],1,[whether we use libjpeg])
                        JPEG_LIBS="-ljpeg"
                        JPEG_CFLAGS=$CPPFLAGS
                ], [
                        LDFLAGS_save="$LDFLAGS"
                        LDFLAGS="-L$jpeg_prefix/lib"
                        AC_CHECK_LIB([jpeg], [jpeg_stdio_src], [
                                have_jpeg=:
                                jpeg_msg="yes (in '$jpeg_prefix')"
                                AC_DEFINE([HAVE_JPEG],1,
                                          [whether we use libjpeg])
                                JPEG_LIBS="-L$jpeg_prefix/lib -ljpeg"
                                JPEG_CFLAGS=$CPPFLAGS
				], [
				jpeg_msg="no (could not link)"
			])
                        LDFLAGS=$LDFLAGS_save
                ])
        ])
        CPPFLAGS="$CPPFLAGS_save"
fi
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
dnl        However, life previews make only sense when you can actually 
dnl        see the preview. This is where aalib fits in - it gives you the
dnl        possibility to view life-previews on any console.
dnl ---------------------------------------------------------------------------
aa_msg="no (http://www.sourceforge.net/projects/aa-project)"
try_aa=:
have_aa=false
AC_ARG_WITH([aalib],AS_HELP_STRING([--without-aalib],[Do not use aalib]),[
        if test x$withval = xno; then
                try_aa=false
        fi
])
if $try_aa; then
        AC_CHECK_HEADER([aalib.h],[
                have_aa=:
                aa_msg=yes
                AC_DEFINE([HAVE_AA], [1], [whether we use aalib])
                AC_PATH_PROG([AALIB_CONFIG], [aalib-config])
                if test -n "${AALIB_CONFIG}"; then
                        AA_CFLAGS=`$AALIB_CONFIG --cflags`
                        AA_LIBS=`$AALIB_CONFIG --libs`
                else
                        AA_LIBS="-laa"
                fi
	])
else
        aa_msg="no (not requested)"
fi
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
try_readline=:
have_readline=false
AC_ARG_WITH([readline],AS_HELP_STRING([--without-readline],
                                      [Do not use readline]),[
        if test "x${withval}" = "xno"; then
                try_readline=false
                rl_msg="no (not requested)"
        fi
])
if $try_readline; then
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
fi
AM_CONDITIONAL([HAVE_RL], [$have_readline])
AC_SUBST([RL_LIBS])
AC_SUBST([RL_CFLAGS])
GP_CONFIG_MSG([Readline support], [$rl_msg])


dnl ---------------------------------------------------------------------------
dnl Turn on all warnings when using gcc
dnl ---------------------------------------------------------------------------
if test "$GCC" = "yes"; then
	AM_CPPFLAGS="$AM_CPPFLAGS -Wall"
	AM_CPPFLAGS="$AM_CPPFLAGS -Wmissing-declarations -Wmissing-prototypes"
	AM_LDFLAGS="$AM_LDFLAGS -Wall"
	# FIXME: Check for --enable-debug here
	AM_CPPFLAGS="$AM_CPPFLAGS -g"
	AM_LDFLAGS="$AM_LDFLAGS -g"
fi

AM_CPPFLAGS="$AM_CPPFLAGS -D_GPHOTO2_INTERNAL_CODE"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_LDFLAGS])


# ---------------------------------------------------------------------------
# Create output files
# ---------------------------------------------------------------------------
AC_CONFIG_FILES([
po/Makefile.in 
Makefile
gphoto2/Makefile
m4m/Makefile
doc/Makefile
contrib/Makefile
packaging/Makefile
packaging/rpm/Makefile
packaging/rpm/package.spec
])
AC_OUTPUT()dnl

# Status output
GP_CONFIG_OUTPUT
