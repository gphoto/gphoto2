SUBDIRS = doc gphoto2 po

########################################################################
# The following rules are only interesting if you want to create
# distribution packages (as source tarballs or system specific packages)

# create .tar.bz2 distribution package for convenience if the
# $(TAR) on this system supports it. else do nothing.
dist-hook:
	if [ "x${CDPATH}" != "x" ]; then \
		echo "You have set CDPATH. If make dist fails, try to unset CDPATH."; \
	fi
	-$(MAKE) dist-bzip2

dist-bzip2:
	-chmod -R a+r $(distdir)
	-tar -c --bzip2 -h -o -f $(distdir).tar.bz2 $(distdir)
