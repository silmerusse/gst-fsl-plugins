if LTIBBUILD

else
FBHEADER_PATH=../../../../inc/misc
endif
plugin_LTLIBRARIES = libmfw_gst_ipu_csc.la 

libmfw_gst_ipu_csc_la_SOURCES =  mfw_gst_ipu_csc.c 
libmfw_gst_ipu_csc_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer  -I../../../../inc/plugin   -I../../../../  
libmfw_gst_ipu_csc_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_LIBS) $(IPU_LIBS) ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_ipu_csc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

if PLATFORM_IS_MX5X
libmfw_gst_ipu_csc_la_CFLAGS += -DIPULIB
libmfw_gst_ipu_csc_la_LIBADD += -lipu 
endif

libmfw_gst_ipu_csc_la_CFLAGS += $(IPU_CFLAGS)
libmfw_gst_ipu_csc_la_LIBADD += $(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) 
libmfw_gst_ipu_csc_la_CFLAGS += -I$(FBHEADER_PATH)

noinst_HEADERS = mfw_gst_ipu_csc.h
