if PLATFORM_IS_MX37
GST_PLATFORM_FLAGS = -DVPU_MX37 -DMPEG2
endif
if PLATFORM_IS_MX51
GST_PLATFORM_FLAGS = -DVPU_MX51 -DVPU_MX5X -DMPEG2
endif
if PLATFORM_IS_MX53
GST_PLATFORM_FLAGS = -DVPU_MX51 -DVPU_MX5X -DMPEG2
endif
if PLATFORM_IS_MX5X
GST_PLATFORM_FLAGS = -DVPU_MX51 -DVPU_MX5X -DMPEG2
endif
if PLATFORM_IS_MX6X
GST_PLATFORM_FLAGS = -DVPU_MX51 -DVPU_MX5X -DMPEG2
endif
if PLATFORM_IS_MX27
GST_PLATFORM_FLAGS = -DVPU_MX27
endif

# change libvpu_dec.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_vpu_dec.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_vpu_dec_la_SOURCES = mfw_gst_vpu_decoder.c mfw_gst_vpu_thread.c

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_vpu_dec_la_CFLAGS = $(GST_BASE_CFLAGS)  -O2 $(VPU_CFLAGS) -I../../../../inc/plugin $(GST_PLATFORM_FLAGS) -DREALMEDIA -DDIVX -I../../../../libs/me -I../../../../libs/gstbufmeta
libmfw_gst_vpu_dec_la_LIBADD =  $(GST_BASE_LIBS) $(GST_LIBS) -lvpu -lgstvideo-0.10
libmfw_gst_vpu_dec_la_LIBADD += ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_vpu_dec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(VPU_LIBS)
# headers we need but don't want installed
noinst_HEADERS = mfw_gst_vpu_decoder.h mfw_gst_vpu_thread.h
