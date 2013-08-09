plugin_LTLIBRARIES = libmfw_gst_spdiftx.la 

libmfw_gst_spdiftx_la_SOURCES =  mfw_gst_spdiftx.c 
libmfw_gst_spdiftx_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -I../../../../inc/plugin
libmfw_gst_spdiftx_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS)
libmfw_gst_spdiftx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -lgstriff-@GST_MAJORMINOR@

noinst_HEADERS = mfw_gst_spdiftx.h
