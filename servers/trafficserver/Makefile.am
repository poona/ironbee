ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

if BUILD_TS_SERVER
plugin_LTLIBRARIES = libloader.la ts_ironbee.la

libloader_la_SOURCES = libloader.c
libloader_la_CPPFLAGS = $(AM_CPPFLAGS) @TS_CPPFLAGS@
libloader_la_LDFLAGS = -module -avoid-version

ts_ironbee_la_SOURCES = ironbee.c
ts_ironbee_la_CPPFLAGS = $(AM_CPPFLAGS) @TS_CPPFLAGS@
ts_ironbee_la_LDFLAGS = -module -avoid-version

install-exec-hook: $(plugin_LTLIBRARIES)
	@echo "Removing unused static libraries..."; \
	for m in $(plugin_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(plugindir)/$$base.*a; \
	  echo TODO: install; \
	done

uninstall-local: $(plugin_LTLIBRARIES)
	@echo "Uninstalling Traffic Server Modules..."; \
	for m in $(plugin_LTLIBRARIES); do \
	  base=`echo $$m | sed 's/\..*//'`; \
	  rm -f $(DESTDIR)$(plugindir)/$$base.*so; \
	done
endif
