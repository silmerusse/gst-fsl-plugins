

# change libvpu_enc.la to something more suitable
plugin_LTLIBRARIES = libmfw_gst_vpu_enc.la

# for the next set of variables, rename the prefix if you renamed the .la

# sources used to compile this plug-in
libmfw_gst_vpu_enc_la_SOURCES = mfw_gst_vpu_encoder.c 

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libmfw_gst_vpu_enc_la_CFLAGS = $(GST_BASE_CFLAGS) -O2 $(VPU_CFLAGS) -I../../../../inc/plugin -DVPU_$(PLATFORM) -I../../../../libs/gstbufmeta -I../../../../libs/vss -I../../../../libs/hbuf_alloc -I../../../../libs/me

libmfw_gst_vpu_enc_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lvpu 
libmfw_gst_vpu_enc_la_LIBADD += ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_vpu_enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(VPU_LIBS) -lgstriff-@GST_MAJORMINOR@
# headers we need but don't want installed
noinst_HEADERS = mfw_gst_vpu_encoder.h vpu_jpegtable.h
