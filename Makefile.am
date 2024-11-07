ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

# Note: The order of directories in this list isn't arbitrary
SUBDIRS = \
	panel-plugin \
	po \
	icons

if DARWIN
# Darwin cctools ar cannot archive nothing, since xfce4++ contains nothing
SUBDIRS +=
else
SUBDIRS += \
	xfce4++ \
	xfce4++/util
endif

distclean-local:
	rm -rf *.cache *~

AUTOMAKE_OPTIONS = 1.8 dist-bzip2

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST = \
	README.md \
	$(NULL)
