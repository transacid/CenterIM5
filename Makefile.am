ACLOCAL_AMFLAGS = -I m4

SUBDIRS = contrib cppconsui doc misc plugins po src tests

EXTRA_DIST = \
	bootstrap \
	HACKING \
	gettext.h

dist_man_MANS = centerim5.1

BUILT_SOURCES = $(top_srcdir)/.version
# we want to execute this target everytime make is run
.PHONY: $(top_srcdir)/.version
$(top_srcdir)/.version:
	@cd $(top_srcdir); version=`misc/git-version-gen .tarball-version`; \
	if test "$$version" != "`cat .version 2> /dev/null`"; then printf '%s' "$$version" > .version; fi

dist-hook:
	@cd $(top_srcdir); misc/git-version-gen .tarball-version > .tarball-version
	@mv $(top_srcdir)/.tarball-version $(top_distdir)

.PHONY: doc
doc:
	cd doc; $(MAKE) doc

# package in all cmake files
EXTRA_DIST += \
	CMakeLists.txt \
	config.h.cmake \
	cmake/CMakeConfs.cmake \
	cmake/FindCursesw.cmake \
	cppconsui/CMakeLists.txt \
	src/CMakeLists.txt \
	tests/CMakeLists.txt \
	plugins/CMakeLists.txt \
	po/CMakeLists.txt \
	doc/CMakeLists.txt
