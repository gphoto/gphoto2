## Process this file with automake to create a GNU Makefile

EXTRA_DIST =
bin_PROGRAMS =
doc_DATA =


########################################################################
# Definitions common to the gphoto2 CLI and gphoto2-webapi executables
########################################################################

common_cflags        = $(AM_CFLAGS)
common_cppflags      = $(AM_CPPFLAGS)
common_dependencies  =
common_ldadd         =
common_ldflags       =
common_sources       =

common_cppflags     += -I$(srcdir)/mongoose

common_cppflags     += -I$(top_srcdir)
common_cppflags     += -I$(top_srcdir)/gphoto2


########################################################################
# The gphoto2 command line interface executable
########################################################################

bin_PROGRAMS         += gphoto2

gphoto2_CFLAGS        = $(common_cflags)
gphoto2_CPPFLAGS      = $(common_cppflags)
gphoto2_DEPENDENCIES  = $(common_dependecies)
gphoto2_LDADD         = $(common_ldadd)
gphoto2_LDFLAGS       = $(common_ldflags)
gphoto2_SOURCES       = $(common_sources)

gphoto2_SOURCES      += actions.c actions.h
gphoto2_SOURCES      += foreach.c foreach.h
gphoto2_SOURCES      += globals.h
gphoto2_SOURCES      += i18n.h
gphoto2_SOURCES      += gp-params.c gp-params.h
gphoto2_SOURCES      += spawnve.c spawnve.h
gphoto2_SOURCES      += main.c main.h
gphoto2_SOURCES      += version.c version.h
gphoto2_SOURCES      += range.c range.h
gphoto2_SOURCES      += shell.c shell.h

if HAVE_CDK
gphoto2_SOURCES      += gphoto2-cmd-config.c gphoto2-cmd-config.h
endif

if HAVE_AA
gphoto2_SOURCES      += gphoto2-cmd-capture.c gphoto2-cmd-capture.h
endif

# gphoto2_LDFLAGS    += -export-dynamic

gphoto2_CPPFLAGS     += -DPROGNAME=\"gphoto2\"

gphoto2_CFLAGS       += $(LIBGPHOTO2_CFLAGS)
gphoto2_CFLAGS       += $(AM_CPPFLAGS)
gphoto2_CFLAGS       += $(CPPFLAGS)
gphoto2_CFLAGS       += $(CDK_CFLAGS)
gphoto2_CFLAGS       += $(AA_CFLAGS)
gphoto2_CFLAGS       += $(LIBEXIF_CFLAGS)
gphoto2_CFLAGS       += $(RL_CFLAGS)
gphoto2_CFLAGS       += $(POPT_CFLAGS)

gphoto2_LDADD        += $(LIBGPHOTO2_LIBS)
gphoto2_LDADD        += $(CDK_LIBS)
gphoto2_LDADD        += $(AA_LIBS)
gphoto2_LDADD        += $(JPEG_LIBS)
gphoto2_LDADD        += $(PTHREAD_LIBS)
gphoto2_LDADD        += $(LIBEXIF_LIBS)
gphoto2_LDADD        += $(INTLLIBS)
gphoto2_LDADD        += $(RL_LIBS)
gphoto2_LDADD        += $(POPT_LIBS)


########################################################################
# The gphoto2-webapi Web API HTTP server
########################################################################

bin_PROGRAMS                += gphoto2-webapi

gphoto2_webapi_CFLAGS        = $(common_cflags)
gphoto2_webapi_CPPFLAGS      = $(common_cppflags)
gphoto2_webapi_DEPENDENCIES  = $(common_dependencies)
gphoto2_webapi_LDADD         = $(common_ldadd)
gphoto2_webapi_LDFLAGS       = $(common_ldflags)
gphoto2_webapi_SOURCES       = $(common_sources)

gphoto2_webapi_SOURCES      += actions.c actions.h
gphoto2_webapi_SOURCES      += foreach.c foreach.h
gphoto2_webapi_SOURCES      += globals.h
gphoto2_webapi_SOURCES      += i18n.h
gphoto2_webapi_SOURCES      += gp-params.c gp-params.h
gphoto2_webapi_SOURCES      += spawnve.c spawnve.h
gphoto2_webapi_SOURCES      += gphoto2-webapi.c gphoto2-webapi.h
gphoto2_webapi_SOURCES      += version.c version.h
gphoto2_webapi_SOURCES      += range.c range.h
gphoto2_webapi_SOURCES      += mongoose/mongoose.c mongoose/mongoose.h
gphoto2_webapi_SOURCES      += server.c server.h

if HAVE_CDK
gphoto2_webapi_SOURCES      += gphoto2-cmd-config.c gphoto2-cmd-config.h
endif

if HAVE_AA
gphoto2_webapi_SOURCES      += gphoto2-cmd-capture.c gphoto2-cmd-capture.h
endif

gphoto2_webapi_CPPFLAGS     += -DPROGNAME=\"gphoto2-webapi\"
gphoto2_webapi_CPPFLAGS     += -DGPHOTO2_WEBAPI

gphoto2_webapi_CFLAGS       += $(LIBGPHOTO2_CFLAGS)
gphoto2_webapi_CFLAGS       += $(AM_CPPFLAGS)
gphoto2_webapi_CFLAGS       += $(CPPFLAGS)
gphoto2_webapi_CFLAGS       += $(CDK_CFLAGS)
gphoto2_webapi_CFLAGS       += $(AA_CFLAGS)
gphoto2_webapi_CFLAGS       += $(LIBEXIF_CFLAGS)
gphoto2_webapi_CFLAGS       += $(RL_CFLAGS)
gphoto2_webapi_CFLAGS       += $(POPT_CFLAGS)

gphoto2_webapi_LDADD        += $(LIBGPHOTO2_LIBS)
gphoto2_webapi_LDADD        += $(CDK_LIBS)
gphoto2_webapi_LDADD        += $(AA_LIBS)
gphoto2_webapi_LDADD        += $(JPEG_LIBS)
gphoto2_webapi_LDADD        += $(PTHREAD_LIBS)
gphoto2_webapi_LDADD        += $(LIBEXIF_LIBS)
gphoto2_webapi_LDADD        += $(INTLLIBS)
gphoto2_webapi_LDADD        += $(RL_LIBS)
gphoto2_webapi_LDADD        += $(POPT_LIBS)


########################################################################
# Some tests
########################################################################

TESTS =
TESTS_ENVIRONMENT = env
check_PROGRAMS =


EXTRA_DIST        += test-hook.sh
doc_DATA          += test-hook.sh
TESTS_ENVIRONMENT += GPHOTO_HOOK=$(srcdir)/test-hook.sh


TESTS             += spawntest
check_PROGRAMS    += spawntest
spawntest_SOURCES  =
spawntest_SOURCES += spawntest.c
spawntest_SOURCES += spawnve.c   spawnve.h
