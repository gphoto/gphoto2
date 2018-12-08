## Process this file with automake to create a GNU Makefile

bin_PROGRAMS = gphoto2

dist_doc_DATA = test-hook.sh

gphoto2_SOURCES =
gphoto2_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-D_GPHOTO2_INTERNAL_CODE
gphoto2_CFLAGS = \
	-I$(top_srcdir) -I$(top_srcdir)/gphoto2		\
	$(LIBGPHOTO2_CFLAGS)				\
	$(PTHREAD_CFLAGS) $(LIBEXIF_CFLAGS) $(RL_CFLAGS) $(POPT_CFLAGS)
gphoto2_LDADD = \
	$(LIBGPHOTO2_LIBS) \
	$(PTHREAD_LIBS) $(LIBEXIF_LIBS) $(INTLLIBS) $(RL_LIBS) $(POPT_LIBS)

if HAVE_AA
gphoto2_SOURCES += gphoto2-cmd-capture.c gphoto2-cmd-capture.h
gphoto2_CFLAGS += $(AA_CFLAGS) $(JPEG_CFLAGS)
gphoto2_LDADD += $(AA_LIBS) $(JPEG_LIBS)
endif

if HAVE_CDK
gphoto2_SOURCES += gphoto2-cmd-config.c gphoto2-cmd-config.h
gphoto2_CFLAGS += $(CDK_CFLAGS)
gphoto2_LDADD += $(CDK_LIBS)
endif

gphoto2_SOURCES += 		\
	i18n.h 			\
	actions.c actions.h 	\
	foreach.c foreach.h 	\
	globals.h 		\
	gp-params.c gp-params.h	\
	spawnve.c spawnve.h	\
	main.c main.h 		\
	version.c version.h	\
	range.c range.h 	\
	shell.c shell.h 

TESTS = spawntest

TESTS_ENVIRONMENT = GPHOTO_HOOK=$(srcdir)/test-hook.sh

check_PROGRAMS = spawntest

spawntest_SOURCES = spawntest.c spawnve.c spawnve.h
