## Process this file with automake to create a GNU Makefile

# We need to add the source files to the list because sometimes 
# people without aalib or cdk make dist.
EXTRA_DIST = \
	gphoto2-cmd-config.c gphoto2-cmd-config.h	\
	gphoto2-cmd-capture.c gphoto2-cmd-capture.h

bin_PROGRAMS = gphoto2

INCLUDES =						\
	-DGPHOTO2_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)/gphoto2				\
	$(CDK_CFLAGS) $(GP_CFLAGS) $(AA_CFLAGS)		\
	$(EXIF_CFLAGS) $(RL_CFLAGS) $(POPT_CFLAGS)

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

if HAVE_POPT
NO_POPT_FILES = 
else
NO_POPT_FILES = options.c options.h
endif

gphoto2_SOURCES = 		\
	$(AA_FILES)		\
	$(CDK_FILES)		\
	$(NO_POPT_FILES)	\
	actions.c actions.h 	\
	foreach.c foreach.h 	\
	globals.h 		\
	main.c main.h 		\
	range.c range.h 	\
	shell.c shell.h 

gphoto2_LDFLAGS = -export-dynamic

gphoto2_LDADD =								\
	$(CDK_LIBS) $(AA_LIBS) $(JPEG_LIBS) $(PTHREAD_LIBS)		\
	$(EXIF_LIBS) $(INTLLIBS) $(RL_LIBS) $(GP_LIBS) $(POPT_LIBS)
