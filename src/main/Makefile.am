bin_PROGRAMS = \
	quetoo \
	quetoo-dedicated

quetoo_SOURCES = \
	main.c

quetoo_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@

quetoo_LDADD = \
	../client/libclient.la \
	../server/libserver.la
	
quetoo_dedicated_SOURCES = \
	main.c

quetoo_dedicated_CFLAGS = \
	-I$(top_srcdir)/src \
	@BASE_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@SDL2_CFLAGS@

quetoo_dedicated_LDADD = \
	../client/libclient_null.la \
	../server/libserver.la

if WINDOWS

.rc.o:
	@WINDRES@ $^ -o $@
        
quetoo_SOURCES += \
	quetoo-icon.rc

quetoo_dedicated_SOURCES += \
	quetoo-dedicated-icon.rc

endif
