# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_wmv9mpdec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_wmv9mpdec_la_SOURCES =  mfw_gst_wmv9mpdec.c 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_wmv9mpdec_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer -fPIC -DOUTPUT_BUFFER_CHANGES -DPADDED_OUTPUT $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin -DDIRECT_RENDER_VERSION=2 -DFRAMEDROPING_ENALBED -I../../../../libs/gstbufmeta 
libmfw_gst_wmv9mpdec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -l_WMV9MP_dec_MP_arm11_elinux ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_wmv9mpdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) -lgstriff-@GST_MAJORMINOR@

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_wmv9mpdec.h 
