## Process this file with automake to create a GNU Makefile

# We need to add the source files to the list because 
# sometimes people without aalib or cdk make dist.
EXTRA_DIST = \
	gphoto2-cmd-config.c gphoto2-cmd-config.h	\
	gphoto2-cmd-capture.c gphoto2-cmd-capture.h	\
	i18n.h test-hook.sh

bin_PROGRAMS = gphoto2

doc_DATA = test-hook.sh

if HAVE_CDK
CDK_FILES = gphoto2-cmd-config.c gphoto2-cmd-config.h
else
CDK_FILES =
endif

if HAVE_AA
AA_FILES = gphoto2-cmd-capture.c gphoto2-cmd-capture.h
else
AA_FILES =
endif

gphoto2_SOURCES = 		\
	$(AA_FILES)		\
	$(CDK_FILES)		\
	$(NO_POPT_FILES)	\
	actions.c actions.h 	\
	foreach.c foreach.h 	\
	globals.h 		\
	gp-params.c gp-params.h	\
	spawnve.c spawnve.h	\
	main.c main.h 		\
	version.c version.h	\
	range.c range.h 	\
	shell.c shell.h 

#gphoto2_LDFLAGS = -export-dynamic

gphoto2_CFLAGS =					\
	-I$(top_srcdir) -I$(top_srcdir)/gphoto2		\
	$(LIBGPHOTO2_CFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS)	\
	$(CDK_CFLAGS) $(AA_CFLAGS)			\
	$(LIBEXIF_CFLAGS) $(RL_CFLAGS) $(POPT_CFLAGS)


gphoto2_LDADD =								\
	$(LIBGPHOTO2_LIBS) $(CDK_LIBS) $(AA_LIBS) $(JPEG_LIBS) 		\
	$(PTHREAD_LIBS) $(LIBEXIF_LIBS) $(INTLLIBS) $(RL_LIBS) $(POPT_LIBS)

TESTS = spawntest

TESTS_ENVIRONMENT = GPHOTO_HOOK=$(srcdir)/test-hook.sh

check_PROGRAMS = spawntest

spawntest_SOURCES = spawntest.c spawnve.c spawnve.h
