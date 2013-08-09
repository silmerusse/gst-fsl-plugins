CORELIB=_oggvorbis_dec_arm11_elinux

# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_vorbisdec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_vorbisdec_la_SOURCES =  mfw_gst_vorbisdec.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_vorbisdec_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 -DMPEG4 -DARM_OPT_MACROS -DLC -DPUSH_MODE -fno-omit-frame-pointer -fPIC
libmfw_gst_vorbisdec_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin


libmfw_gst_vorbisdec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_MAJORMINOR) -l$(CORELIB)
libmfw_gst_vorbisdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) -lgstriff-@GST_MAJORMINOR@

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_vorbisdec.h
