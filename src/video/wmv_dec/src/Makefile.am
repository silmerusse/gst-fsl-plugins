# plugindir is set in configure
plugin_LTLIBRARIES = libmfw_gst_wmvdec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libmfw_gst_wmvdec_la_SOURCES =  mfw_gst_wmvdec.c 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_wmvdec_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_BASE_CFLAGS) -O2 -fno-omit-frame-pointer -fPIC -DOUTPUT_BUFFER_CHANGES -DWMV9_SIMPLE_ONLY -DPADDED_OUTPUT $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
libmfw_gst_wmvdec_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -l_WMV789_dec_arm11_elinux
libmfw_gst_wmvdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) -lgstriff-@GST_MAJORMINOR@

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_wmvdec.h 
