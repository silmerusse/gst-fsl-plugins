# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_mpeg2dec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_mpeg2dec_la_SOURCES =  mfw_gst_mpeg2dec.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_mpeg2dec_la_CFLAGS = $(GST_BASE_CFLAGS) -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -DDIRECT_RENDER_VERSION=2 -I../../../../libs/me -I../../../../libs/gstbufmeta
libmfw_gst_mpeg2dec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -l_mpeg2_dec_arm11_elinux
libmfw_gst_mpeg2dec_la_LIBADD +=  ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_mpeg2dec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_mpeg2dec.h
