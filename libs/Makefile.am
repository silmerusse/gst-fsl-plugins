lib_LTLIBRARIES = libgstfsl-@GST_MAJORMINOR@.la


if PLATFORM_IS_MX5X
libgstfsl_@GST_MAJORMINOR@_la_SOURCES = \
    gstbufmeta/gstbufmeta.c \
    gstnext/gstnext.c       \
    gstsutils/gstsutils.c   \
    sconf/mfw_gst_sconf.c   \
    hbuf_alloc/hwbuffer_allocator.c \
    me/mfw_gst_ts.c         \
    vss/mfw_gst_vss_common.c\
    vss/mfw_gst_video_surface.c

else
if PLATFORM_IS_MX6X
libgstfsl_@GST_MAJORMINOR@_la_SOURCES = \
    gstbufmeta/gstbufmeta.c \
    gstnext/gstnext.c       \
    gstsutils/gstsutils.c   \
    sconf/mfw_gst_sconf.c   \
    hbuf_alloc/hwbuffer_allocator.c \
    me/mfw_gst_ts.c         \
    vss/mfw_gst_vss_common.c\
    vss/mfw_gst_video_surface.c

else
libgstfsl_@GST_MAJORMINOR@_la_SOURCES = \
    gstbufmeta/gstbufmeta.c \
    gstnext/gstnext.c       \
    gstsutils/gstsutils.c   \
    sconf/mfw_gst_sconf.c   \
    me/mfw_gst_ts.c
endif
endif


libgstfsl_@GST_MAJORMINOR@_la_CFLAGS  = $(GST_BASE_CFLAGS) $(IPU_CFLAGS) -I../inc/common
libgstfsl_@GST_MAJORMINOR@_la_LDFLAGS = -version-info @GSTMXLIBS_VERSION@
libgstfsl_@GST_MAJORMINOR@_la_LIBADD = $(GST_BASE_LIBS) -lgstvideo-@GST_MAJORMINOR@ -lgstinterfaces-$(GST_MAJORMINOR) $(IPU_LIBS)

libgstfsl_@GST_MAJORMINOR@includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/imx-mm
libgstfsl_@GST_MAJORMINOR@include_HEADERS = \
    gstbufmeta/gstbufmeta.h     \
    gstnext/gstnext.h           \
    gstsutils/gstsutils.h       \
    sconf/mfw_gst_sconf.h       \
    me/mfw_gst_ts.h             \
    vss/mfw_gst_vss_common.h    \
    vss/mfw_gst_video_surface.h

data_DATA = vss/vssconfig vss/vssconfig.dvi_tv vss/vssconfig.dvi_wvga
EXTRA_DIST = $(data_DATA)
