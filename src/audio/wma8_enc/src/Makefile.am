# plugindir is set in configure


# change libgstplugin.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_wma8enc.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_wma8enc_la_SOURCES = mfw_gst_wma8enc.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libmfw_gst_wma8enc_la_CFLAGS = $(GST_CFLAGS) $(FSL_MM_CORE_CFLAGS)  -I../../../../inc/plugin
libmfw_gst_wma8enc_la_LIBADD = $(GST_LIBS) -l_wma_muxer_arm11_ELINUX -lgstbase-$(GST_MAJORMINOR) -l_wma8_enc_arm11_elinux 
libmfw_gst_wma8enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS) 

# headers we need but don't want installed
noinst_HEADERS = mfw_gst_wma8enc.h tchar.h
