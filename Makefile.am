SUBDIRS = gphoto-m4 contrib doc gphoto2 packaging po tests

ACLOCAL_AMFLAGS = -I auto-m4 -I gphoto-m4
EXTRA_DIST = README.md

README: $(srcdir)/README.md
	cp $(srcdir)/README.md README

@GP_GETTEXT_SETUP_MK@

# Dummy target to force Automake to make the "all" target depend on it
all-local:
	@:

# Dummy target to force Automake to make the "distclean" target depend on it
distclean-local:
	@:
