if LTIBBUILD
else
FBHEADER_PATH=../../../../inc/misc
endif

plugin_LTLIBRARIES = libmfw_gst_isink.la 

libmfw_gst_isink_la_SOURCES =  mfw_gst_isink.c 
libmfw_gst_isink_la_CFLAGS = -O2 $(GST_BASE_CFLAGS) -fPIC -fno-omit-frame-pointer $(IPU_CFLAGS) -D_$(PLATFORM) -I../../../../inc/plugin -I../../../../inc/common -I../../../../libs/vss -I../../../../libs/gstbufmeta -I../../../../libs/hbuf_alloc -I$(FBHEADER_PATH) -I. -I/usr/src/linux-headers-2.6.35-1000-linaro-imx5/include
libmfw_gst_isink_la_LIBADD = $(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR) 
libmfw_gst_isink_la_LIBADD += ../../../../libs/libgstfsl-@GST_MAJORMINOR@.la
libmfw_gst_isink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) 

if USE_X11
libmfw_gst_isink_la_SOURCES += mfw_gst_xlib.c mfw_gst_i_xlib.c
libmfw_gst_isink_la_LIBADD += -lX11 -lXext
libmfw_gst_isink_la_CFLAGS += -DUSE_X11 -I/usr/src/linux-headers-2.6.35-1000-linaro-imx5/include
endif

noinst_HEADERS = \
    mfw_gst_isink.h     \
    mfw_gst_i_xlib.h    \
    mfw_gst_xlib.h      \
    mfw_isink_frame.h
