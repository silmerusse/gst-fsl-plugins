plugin_LTLIBRARIES = libmfw_gst_deinterlace.la 

libmfw_gst_deinterlace_la_SOURCES =  mfw_gst_deinterlace.c 
libmfw_gst_deinterlace_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer -I../../../../inc/plugin $(FSL_MM_CORE_CFLAGS)
libmfw_gst_deinterlace_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -l_deinterlace_arm11_elinux
libmfw_gst_deinterlace_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

noinst_HEADERS = mfw_gst_deinterlace.h
