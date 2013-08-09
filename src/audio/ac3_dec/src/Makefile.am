# plugindir is set in configure

# change libfsl_ac3dec.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_ac3dec.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_ac3dec_la_SOURCES = mfw_gst_ac3dec.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_ac3dec_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
libmfw_gst_ac3dec_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_MAJORMINOR) -l_ac3_dec_arm11_elinux 
libmfw_gst_ac3dec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)   $(FSL_MM_CORE_LIBS) -lgstriff-@GST_MAJORMINOR@
# headers we need but don't want installed
noinst_HEADERS = mfw_gst_ac3dec.h
