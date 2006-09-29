# packaging/rpm/Makefile.am
# Create an RPM package out of an automake based source distribution.
# © 2001,2003 Hans Ulrich Niedermann <gp@n-dimensional.de>

EXTRA_DIST = package.spec.in @PACKAGE@.spec

clean-local:
	rm -rf BUILD RPMS SPECS SOURCES SRPMS

DISTCLEANFILES = @PACKAGE@.spec

@PACKAGE@.spec: package.spec
	cp $< $@

rpm: Makefile package.spec
	$(MAKE) clean
	mkdir -p BUILD SPECS SOURCES SRPMS
	for arch in i386 i486 i586 i686 alpha noarch; do \
		mkdir -p "RPMS/$${arch}"; \
	done
	ln -f "$(top_srcdir)/@PACKAGE@-@VERSION@.tar.gz" SOURCES/
	ln -f package.spec SPECS/@PACKAGE@-@VERSION@.spec
	rpm --define "_topdir $$(pwd)" -ba SPECS/@PACKAGE@-@VERSION@.spec
