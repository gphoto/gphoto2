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

# Where to get mongoose from
MONGOOSE_REPO    = https://github.com/cesanta/mongoose.git
MONGOOSE_RELEASE = 7.7

# The files to copy from the upstream mongoose repo to gphoto2/mongoose/
MONGOOSE_FILES  =
MONGOOSE_FILES += README.md
MONGOOSE_FILES += LICENSE
MONGOOSE_FILES += mongoose.c
MONGOOSE_FILES += mongoose.h

# Update local copy of mongoose source to latest mongoose release
update-mongoose:
	@set -ex; \
	test -f "$(srcdir)/.git/config"; \
	if test -f mongoose-repo/.git/config; then \
	  (cd mongoose-repo; git fetch -v); \
	else \
	  git clone '$(MONGOOSE_REPO)' mongoose-repo; \
	fi; \
	(cd mongoose-repo; \
	  latest="$$(git tag --sort=version:refname | tail -n1)"; \
	  if test "x$(MONGOOSE_RELEASE)" = "x$$latest"; then :; else \
	    echo "The latest upstream mongoose release is $$latest."; \
	    echo "We still have MONGOOSE_RELEASE=$(MONGOOSE_RELEASE) in Makefile.am."; \
	    echo "To update our mongoose copy to $$latest, update Makefile.am to"; \
	    echo "MONGOOSE_RELEASE=$${latest} and re-run 'make update-mongoose'."; \
	    exit 1; \
	  fi; \
	  git checkout "$$latest" ); \
	for f in $(MONGOOSE_FILES); do \
	  cp -f "mongoose-repo/$$f" "$(srcdir)/gphoto2/mongoose/$$f"; \
	done; \
	(cd "$(srcdir)"; git diff gphoto2/mongoose)
