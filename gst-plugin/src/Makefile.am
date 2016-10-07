# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstpanorama.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

## Plugin 1

# sources used to compile this plug-in
#libgstplugin_la_SOURCES = gstplugin.c gstplugin.h

# compiler and linker flags used to compile this plugin, set in configure.ac
#libgstplugin_la_CFLAGS = $(GST_CFLAGS)
#libgstplugin_la_LIBADD = $(GST_LIBS)
#libgstplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
#libgstplugin_la_LIBTOOLFLAGS = --tag=disable-static

## Plugin 2 (audio filter example)

# sources used to compile this plug-in
libgstpanorama_la_SOURCES = gstpanorama.c gstPanoramaplugin.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstpanorama_la_CFLAGS = $(GST_CFLAGS) -I/usr/local/cuda-7.5/samples/common/inc
libgstpanorama_la_LIBADD = $(GST_LIBS) -L./ -lpano
libgstpanorama_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstpanorama_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstplugin.h
