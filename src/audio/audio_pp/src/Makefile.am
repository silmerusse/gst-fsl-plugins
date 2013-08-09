plugin_LTLIBRARIES = libmfw_gst_audio_pp.la 

libmfw_gst_audio_pp_la_SOURCES =  mfw_gst_audio_pp.c 
libmfw_gst_audio_pp_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer $(FSL_MM_CORE_CFLAGS) -I../../../../inc/plugin
libmfw_gst_audio_pp_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) -l_peq_arm11_elinux
libmfw_gst_audio_pp_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(FSL_MM_CORE_LIBS)

noinst_HEADERS = mfw_gst_audio_pp.h
