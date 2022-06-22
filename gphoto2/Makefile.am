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

common_sources      += globals.h
common_sources      += i18n.h
common_sources      += actions.c   actions.h
common_sources      += foreach.c   foreach.h
common_sources      += gp-params.c gp-params.h
common_sources      += spawnve.c   spawnve.h
common_sources      += version.c   version.h
common_sources      += range.c     range.h

common_cppflags     += -I$(srcdir)/mongoose

common_cppflags     += -I$(top_srcdir)
common_cppflags     += -I$(top_srcdir)/gphoto2

common_cppflags     += $(POPT_CFLAGS)
common_ldadd        += $(POPT_LIBS)

common_cppflags     += $(LIBEXIF_CFLAGS)
common_ldadd        += $(LIBEXIF_LIBS)

common_cppflags     += $(LIBGPHOTO2_CFLAGS)
common_ldadd        += $(LIBGPHOTO2_LIBS)

# TODO: Where are the corresponding _cppflags definitions?
common_ldadd        += $(JPEG_LIBS)
common_ldadd        += $(PTHREAD_LIBS)
common_ldadd        += $(INTLLIBS)



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

gphoto2_SOURCES      += main.c main.h
gphoto2_SOURCES      += shell.c shell.h

# gphoto2_LDFLAGS    += -export-dynamic

gphoto2_CPPFLAGS     += -DPROGNAME=\"gphoto2\"

gphoto2_CFLAGS       += $(RL_CFLAGS)
gphoto2_LDADD        += $(RL_LIBS)

if HAVE_CDK
gphoto2_SOURCES      += gphoto2-cmd-config.c gphoto2-cmd-config.h
gphoto2_CFLAGS       += $(CDK_CFLAGS)
gphoto2_LDADD        += $(CDK_LIBS)
endif

if HAVE_AA
gphoto2_SOURCES      += gphoto2-cmd-capture.c gphoto2-cmd-capture.h
gphoto2_CFLAGS       += $(AA_CFLAGS)
gphoto2_LDADD        += $(AA_LIBS)
endif


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

gphoto2_webapi_SOURCES      += gphoto2-webapi.c gphoto2-webapi.h
gphoto2_webapi_SOURCES      += mongoose/mongoose.c mongoose/mongoose.h
gphoto2_webapi_SOURCES      += server.c server.h

gphoto2_webapi_CPPFLAGS     += -DPROGNAME=\"gphoto2-webapi\"
gphoto2_webapi_CPPFLAGS     += -DGPHOTO2_WEBAPI

# FIXME: Does interactive config make sense for gphoto2-webapi?
if HAVE_CDK
gphoto2_webapi_SOURCES      += gphoto2-cmd-config.c gphoto2-cmd-config.h
gphoto2_webapi_CFLAGS       += $(CDK_CFLAGS)
gphoto2_webapi_LDADD        += $(CDK_LIBS)
endif

# FIXME: Does ASCII art preview make sense for gphoto2-webapi?
if HAVE_AA
gphoto2_webapi_SOURCES      += gphoto2-cmd-capture.c gphoto2-cmd-capture.h
gphoto2_webapi_CFLAGS       += $(AA_CFLAGS)
gphoto2_webapi_LDADD        += $(AA_LIBS)
endif


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
